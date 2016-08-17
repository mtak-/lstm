#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <cassert>
#include <map>
#include <set>

LSTM_BEGIN
    namespace detail {
        struct transaction_base {
        protected:
            friend detail::atomic_fn;
            friend test::transaction_tester;

            template<std::nullptr_t = nullptr>
            static std::atomic<word> clock;
            static inline word get_clock() noexcept { return clock<>.load(LSTM_ACQUIRE); }

            word read_version;

            transaction_base() noexcept = default;
            ~transaction_base() noexcept = default;
            
            static inline bool locked(word version) noexcept { return version & 1; }
            static inline word as_locked(word version) noexcept { return version | 1; }

            bool lock(word& version_buf, const detail::var_base& v) const noexcept {
                while (version_buf <= read_version &&
                    !v.version_lock.compare_exchange_weak(version_buf,
                                                          as_locked(version_buf),
                                                          LSTM_RELEASE)); // TODO: is this correct?
                return version_buf <= read_version && !locked(version_buf);
            }

            static void unlock(const word version_to_set, const detail::var_base& v) noexcept {
                assert(locked(v.version_lock.load(LSTM_RELAXED)));
                assert(!locked(version_to_set));
                v.version_lock.store(version_to_set, LSTM_RELEASE);
            }

            static void unlock(const detail::var_base& v) noexcept {
                assert(locked(v.version_lock.load(LSTM_RELAXED)));
                v.version_lock.fetch_sub(1, LSTM_RELEASE);
            }

            bool read_valid(const detail::var_base& v) const noexcept
            { return v.version_lock.load(LSTM_ACQUIRE) <= read_version; }

            virtual void add_read_set(const detail::var_base& v) = 0;
            virtual void add_write_set(detail::var_base& v, var_storage ptr) = 0;
            virtual std::pair<var_storage, bool> find_write_set(const detail::var_base& v) noexcept = 0;

        public:
            transaction_base(const transaction_base&) = delete;
            transaction_base(transaction_base&&) = delete;
            transaction_base& operator=(const transaction_base&) = delete;
            transaction_base& operator=(transaction_base&&) = delete;
            
            // non trivial loads, require locking the var<T> before copying it
            template<typename T,
                LSTM_REQUIRES_(!var<T>::trivial)>
            T load(const var<T>& v) {
                auto write_ptr_success = find_write_set(v);
                if (!write_ptr_success.second) {
                    word version_buf = 0;
                    // TODO: this feels wrong, especially since reads call this...
                    if (lock(version_buf, v)) {
                        // if copying T causes a lock to be taken out on T, then this line will
                        // abort the transaction or cause a stack overflow.
                        // this is ok, because it is certainly a bug in the client code
                        // (me thinks..)
                        auto result = v.unsafe();
                        unlock(version_buf, v);

                        add_read_set(v);
                        return result;
                    }
                } else if (read_valid(v)) // TODO: does this if check improve or hurt speed?
                    return var<T>::load(write_ptr_success.first);
                throw tx_retry{};
            }
            
            // trivial loads are fast :)
            template<typename T,
                LSTM_REQUIRES_(var<T>::trivial)>
            T load(const var<T>& v) {
                auto write_ptr_success = find_write_set(v);
                if (!write_ptr_success.second) {
                    auto v0 = v.version_lock.load(LSTM_ACQUIRE);
                    if (v0 <= read_version && !locked(v0)) {
                        auto result = var<T>::load(v.value);
                        if (v.version_lock.load(LSTM_ACQ_REL) == v0) {
                            add_read_set(v);
                            return result;
                        }
                    }
                } else if (read_valid(v)) // TODO: does this if check improve or hurt speed?
                    return var<T>::load(write_ptr_success.first);
                throw tx_retry{};
            }

            template<typename T, typename U,
                LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                               std::is_constructible<T, U&&>())>
            void store(var<T>& v, U&& u) __attribute__((noinline)){
                auto write_ptr_success = find_write_set(v);
                if (!write_ptr_success.second)
                    add_write_set(v, v.allocate_construct((U&&)u));
                else
                    v.load(write_ptr_success.first) = (U&&)u;

                // TODO: where is this best placed???, or is it best removed altogether?
                if (!read_valid(v)) throw tx_retry{};
            }

            // TODO: reading/writing an rvalue probably never makes sense?
            template<typename T>
            void load(const var<T>&& v) = delete;

            template<typename T>
            void store(var<T>&& v) = delete;
        };

        template<std::nullptr_t>
        std::atomic<word> transaction_base::clock{0};
    }

    template<typename Alloc>
    struct transaction : detail::transaction_base {
    private:
        friend detail::atomic_fn;
        friend test::transaction_tester;

        std::set<const detail::var_base*, std::less<>, Alloc> read_set;
        std::map<detail::var_base*, detail::var_storage, std::less<>, Alloc> write_set;

        transaction(const Alloc& alloc) // TODO: noexcept ?
            : read_set(alloc)
            , write_set(alloc)
        {}

        ~transaction() noexcept = default;

        void commit_lock_writes() {
            auto write_begin = std::begin(write_set);
            word version_buf;
            for (auto iter = write_begin; iter != std::end(write_set); ++iter) {
                version_buf = 0;
                // TODO: only care what version the var is, if it's also a read
                if (!lock(version_buf, *iter->first)) {
                    for (; write_begin != iter; ++write_begin)
                        unlock(*write_begin->first);
                    throw tx_retry{};
                }
                read_set.erase(iter->first); // FIXME: weird to have this here
            }
        }

        void commit_validate_reads() {
            // reads do not need to be locked to be validated
            for (auto& var : read_set) {
                if (!read_valid(*var)) {
                    for (auto& var_val : write_set)
                        unlock(*var_val.first);
                    throw tx_retry{};
                }
            }
        }

        void commit_publish(const word write_version) {
            for (auto& var_val : write_set) {
                var_val.first->destroy_deallocate(var_val.first->value);
                var_val.first->value = var_val.second;
                unlock(write_version, *var_val.first);
            }

            write_set.clear();
        }

        void commit() {
            if (write_set.empty())
                return;
            commit_lock_writes();
            auto write_version = clock<>.fetch_add(2, LSTM_RELEASE) + 2;
            commit_validate_reads();
            commit_publish(write_version);
        }

        void cleanup() noexcept {
            for (auto& var_val : write_set)
                var_val.first->destroy_deallocate(var_val.second);
            write_set.clear();
        }

        void add_read_set(const detail::var_base& v) override final
        { read_set.emplace(std::addressof(v)); }

        void add_write_set(detail::var_base& v, detail::var_storage ptr) override final
        { write_set.emplace(std::addressof(v), ptr); }

        std::pair<detail::var_storage, bool>
        find_write_set(const detail::var_base& v) noexcept override final {
            auto iter = write_set.find(std::addressof(v));
            return iter != std::end(write_set)
              ? std::make_pair(iter->second, true)
              : std::make_pair(nullptr, false);
        }
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <cassert>
#include <vector>

LSTM_BEGIN
    namespace detail {
        struct write_set_storage {
            detail::var_base* var;
            detail::var_storage pending_write;
        };
        
        struct write_set_lookup {
            var_storage storage;
            bool success;
        };
        
        struct transaction_base {
        protected:
            friend detail::atomic_fn;
            friend test::transaction_tester;

            template<std::nullptr_t = nullptr>
            static std::atomic<word> clock;
            static inline word get_clock() noexcept { return clock<>.load(LSTM_ACQUIRE); }

            word read_version;

            inline transaction_base() noexcept
                : read_version(get_clock())
            {}
            
            static inline bool locked(word version) noexcept { return version & 1; }
            static inline word as_locked(word version) noexcept { return version | 1; }

            bool lock(word& version_buf, const detail::var_base& v) const noexcept {
                while (version_buf <= read_version &&
                    !v.version_lock.compare_exchange_weak(version_buf,
                                                          as_locked(version_buf),
                                                          LSTM_RELEASE)); // TODO: is this correct?
                return version_buf <= read_version && !locked(version_buf);
            }

            static inline
            void unlock(const word version_to_set, const detail::var_base& v) noexcept {
                assert(locked(v.version_lock.load(LSTM_RELAXED)));
                assert(!locked(version_to_set));
                v.version_lock.store(version_to_set, LSTM_RELEASE);
            }

            static inline void unlock(const detail::var_base& v) noexcept {
                assert(locked(v.version_lock.load(LSTM_RELAXED)));
                v.version_lock.fetch_sub(1, LSTM_RELEASE); // TODO: seems correct, but might not be
            }

            inline bool read_valid(const detail::var_base& v) const noexcept
            { return v.version_lock.load(LSTM_ACQUIRE) <= read_version; }

            virtual void add_read_set(const detail::var_base& v) = 0;
            virtual void add_write_set(detail::var_base& v, var_storage ptr) = 0;
            virtual write_set_lookup find_write_set(const detail::var_base& v) noexcept = 0;

        public:
            // transactions can only be passed by non-const lvalue reference
            transaction_base(const transaction_base&) = delete;
            transaction_base(transaction_base&&) = delete;
            transaction_base& operator=(const transaction_base&) = delete;
            transaction_base& operator=(transaction_base&&) = delete;
            
            // non trivial loads, require locking the var<T> before copying it
            template<typename T,
                LSTM_REQUIRES_(!var<T>::trivial)>
            T load(const var<T>& v) {
                write_set_lookup lookup = find_write_set(v);
                if (!lookup.success) {
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
                    return var<T>::load(lookup.storage);
                throw tx_retry{};
            }
            
            // trivial loads are fast :)
            template<typename T,
                LSTM_REQUIRES_(var<T>::trivial)>
            T load(const var<T>& v) {
                write_set_lookup lookup = find_write_set(v);
                if (!lookup.success) {
                    // TODO: is this synchronization correct?
                    // or does it thrash the cache too much?
                    auto v0 = v.version_lock.load(LSTM_ACQUIRE);
                    if (v0 <= read_version && !locked(v0)) {
                        auto result = var<T>::load(v.storage);
                        
                        if (v.version_lock.load(LSTM_ACQUIRE) == v0) {
                            add_read_set(v);
                            return result;
                        }
                    }
                } else if (read_valid(v)) // TODO: does this if check improve or hurt speed?
                    return var<T>::load(lookup.storage);
                throw tx_retry{};
            }

            template<typename T, typename U,
                LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                               std::is_constructible<T, U&&>())>
            void store(var<T>& v, U&& u) {
                write_set_lookup lookup = find_write_set(v);
                if (!lookup.success)
                    add_write_set(v, v.allocate_construct((U&&)u));
                else
                    v.load(lookup.storage) = (U&&)u;

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
        using alloc_traits = std::allocator_traits<Alloc>;
        using read_alloc = typename alloc_traits::template rebind_alloc<const detail::var_base*>;
        using write_alloc = typename alloc_traits::template rebind_alloc<detail::write_set_storage>;
        friend detail::atomic_fn;
        friend test::transaction_tester;

        // TODO: make some container choices
        std::vector<const detail::var_base*, read_alloc> read_set;
        std::vector<detail::write_set_storage, write_alloc> write_set;

        inline transaction(const Alloc& alloc) noexcept
            : read_set(alloc)
            , write_set(alloc)
        {}

        void commit_lock_writes() {
            auto write_begin = std::begin(write_set);
            word version_buf;
            for (auto iter = write_begin; iter != std::end(write_set); ++iter) {
                version_buf = 0;
                // TODO: only care what version the var is, if it's also a read
                if (!lock(version_buf, *iter->var)) {
                    for (; write_begin != iter; ++write_begin)
                        unlock(*write_begin->var);
                    throw tx_retry{};
                }
                // FIXME: weird to have this here
                read_set.erase(std::find(std::begin(read_set), std::end(read_set), iter->var));
            }
        }

        void commit_validate_reads() {
            // reads do not need to be locked to be validated
            for (auto& var : read_set) {
                if (!read_valid(*var)) {
                    for (auto& var_val : write_set)
                        unlock(*var_val.var);
                    throw tx_retry{};
                }
            }
        }

        void commit_publish(const word write_version) noexcept {
            for (auto& var_val : write_set) {
                var_val.var->destroy_deallocate(var_val.var->storage);
                var_val.var->storage = var_val.pending_write;
                unlock(write_version, *var_val.var);
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
            cleanup_retry(); // destructor will handle the deallocation...
        }

        void cleanup_retry() noexcept {
            // TODO: destroy only???
            for (auto& var_val : write_set)
                var_val.var->destroy_deallocate(var_val.pending_write);
            write_set.clear();
        }

        // TODO: make this have a reason for being
        inline void cleanup() noexcept {
            cleanup_retry();
        }

        void add_read_set(const detail::var_base& v) override final {
            const auto ptr_v = std::addressof(v);
            const auto end = std::end(read_set);
            const auto iter = std::find(std::begin(read_set), end, ptr_v);
            if (iter == end)
                read_set.push_back({ptr_v});
        }

        void add_write_set(detail::var_base& v, detail::var_storage ptr) override final {
            const auto ptr_v = std::addressof(v);
            const auto end = std::end(write_set);
            const auto iter = std::find_if(
                std::begin(write_set),
                end,
                [&ptr_v](const detail::write_set_storage& rhs) noexcept -> bool
                { return ptr_v == rhs.pending_write; });
            if (iter == end)
                write_set.push_back({std::addressof(v), ptr});
        }

        detail::write_set_lookup find_write_set(const detail::var_base& v) noexcept override final {
            const auto ptr_v = std::addressof(v);
            const auto end = std::end(write_set);
            const auto iter = std::find_if(
                std::begin(write_set),
                end,
                [&ptr_v](const detail::write_set_storage& rhs) noexcept -> bool
                { return ptr_v == rhs.pending_write; });
            return iter != end
                ? detail::write_set_lookup{iter->pending_write, true}
                : detail::write_set_lookup{{}, false};
        }
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

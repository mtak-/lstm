#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>

LSTM_BEGIN
    namespace detail {
        struct read_set_value_type {
        private:
            const var_base* _src_var;
            
        public:
            inline constexpr read_set_value_type(const var_base& in_src_var) noexcept
                : _src_var(&in_src_var)
            {}
                
            inline constexpr const var_base& src_var() const noexcept { return *_src_var; }
            
            inline constexpr bool is_src_var(const var_base& rhs) const noexcept
            { return _src_var == &rhs; }
            
            inline constexpr bool operator<(const read_set_value_type& rhs) const noexcept
            { return _src_var == rhs._src_var; }
        };
        
        struct write_set_value_type {
        private:
            var_base* _dest_var;
            var_storage _pending_write;
            
        public:
            inline constexpr
            write_set_value_type(var_base& in_dest_var, var_storage in_pending_write) noexcept
                : _dest_var(&in_dest_var)
                , _pending_write(std::move(in_pending_write))
            {}
                
            inline constexpr var_base& dest_var() const noexcept { return *_dest_var; }
            inline constexpr var_storage& pending_write() noexcept { return _pending_write; }
            inline constexpr const var_storage& pending_write() const noexcept
            { return _pending_write; }
            
            inline constexpr bool is_dest_var(const var_base& rhs) const noexcept
            { return _dest_var == &rhs; }
            
            inline constexpr bool operator<(const write_set_value_type& rhs) const noexcept
            { return _dest_var < rhs._dest_var; }
        };
        
        struct write_set_lookup {
        private:
            var_storage* _pending_write;
            
        public:
            inline constexpr write_set_lookup(std::nullptr_t) noexcept : _pending_write{nullptr} {}
            
            inline constexpr
            write_set_lookup(var_storage& in_pending_write) noexcept
                : _pending_write{&in_pending_write}
            {}
                
            inline constexpr bool success() const noexcept { return _pending_write != nullptr; }
            inline constexpr var_storage& pending_write() const noexcept { return *_pending_write; }
        };
        
        struct transaction_base {
        protected:
            friend detail::atomic_fn;
            friend test::transaction_tester;

            template<std::nullptr_t = nullptr>
            static std::atomic<word> clock;
            static inline word get_clock() noexcept { return clock<>.load(LSTM_ACQUIRE); }

            word read_version;

            inline transaction_base() noexcept : read_version(get_clock()) {}
            
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
            T load(const var<T>& src_var) {
                write_set_lookup lookup = find_write_set(src_var);
                if (!lookup.success()) {
                    word version_buf = 0;
                    // TODO: this feels wrong, especially since reads call this...
                    if (lock(version_buf, src_var)) {
                        // if copying T causes a lock to be taken out on T, then this line will
                        // abort the transaction or cause a stack overflow.
                        // this is ok, because it is certainly a bug in the client code
                        // (me thinks..)
                        T result = src_var.unsafe();
                        unlock(version_buf, src_var);

                        add_read_set(src_var);
                        return result;
                    }
                } else if (read_valid(src_var)) // TODO: does this if check improve or hurt speed?
                    return var<T>::load(lookup.pending_write());
                throw tx_retry{};
            }
            
            // trivial loads are fast :)
            template<typename T,
                LSTM_REQUIRES_(var<T>::trivial)>
            T load(const var<T>& src_var) {
                write_set_lookup lookup = find_write_set(src_var);
                if (!lookup.success()) {
                    // TODO: is this synchronization correct?
                    // or does it thrash the cache too much?
                    auto src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                    if (src_version <= read_version && !locked(src_version)) {
                        T result = var<T>::load(src_var.storage);
                        
                        if (src_var.version_lock.load(LSTM_ACQUIRE) == src_version) {
                            add_read_set(src_var);
                            return result;
                        }
                    }
                } else if (read_valid(src_var)) // TODO: does this if check improve or hurt speed?
                    return var<T>::load(lookup.pending_write());
                throw tx_retry{};
            }

            template<typename T, typename U,
                LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                               std::is_constructible<T, U&&>())>
            void store(var<T>& dest_var, U&& u) {
                write_set_lookup lookup = find_write_set(dest_var);
                if (!lookup.success())
                    add_write_set(dest_var, dest_var.allocate_construct((U&&)u));
                else
                    dest_var.load(lookup.pending_write()) = (U&&)u;

                // TODO: where is this best placed???, or is it best removed altogether?
                if (!read_valid(dest_var)) throw tx_retry{};
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
        friend detail::atomic_fn;
        friend test::transaction_tester;
    private:
        using alloc_traits = std::allocator_traits<Alloc>;
        using read_alloc = typename alloc_traits
                                ::template rebind_alloc<detail::read_set_value_type>;
        using write_alloc = typename alloc_traits
                                ::template rebind_alloc<detail::write_set_value_type>;
        using read_set_t = std::vector<detail::read_set_value_type, read_alloc>;
        using write_set_t = std::vector<detail::write_set_value_type, write_alloc>;
        using read_set_const_iter = typename read_set_t::const_iterator;

        // TODO: make some container choices
        read_set_t read_set;
        write_set_t write_set;

        inline transaction(const Alloc& alloc) noexcept
            : read_set(alloc)
            , write_set(alloc)
        {}

        void commit_lock_writes() {
            // pretty sure write needs to be sorted
            // in order to have lockfreedom on trivial types?
            auto write_begin = std::begin(write_set);
            auto write_end = std::end(write_set);
            std::sort(write_begin, write_end);
            
            word version_buf;
            for (auto write_iter = write_begin; write_iter != write_end; ++write_iter) {
                version_buf = 0;
                // TODO: only care what version the var is, if it's also a read
                if (!lock(version_buf, write_iter->dest_var())) {
                    for (; write_begin != write_iter; ++write_begin)
                        unlock(write_begin->dest_var());
                    throw tx_retry{};
                }
                
                auto read_iter = read_set_find(write_iter->dest_var());
                if (read_iter != std::end(read_set))
                    read_set.erase(read_iter);
            }
        }

        void commit_validate_reads() {
            // reads do not need to be locked to be validated
            for (auto& read_set_vaue : read_set) {
                if (!read_valid(read_set_vaue.src_var())) {
                    for (auto& write_set_value : write_set)
                        unlock(write_set_value.dest_var());
                    throw tx_retry{};
                }
            }
        }

        void commit_publish(const word write_version) noexcept {
            for (auto& write_set_value : write_set) {
                write_set_value.dest_var().destroy_deallocate(write_set_value.dest_var().storage);
                write_set_value.dest_var().storage = std::move(write_set_value.pending_write());
                unlock(write_version, write_set_value.dest_var());
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
            cleanup();
        }

        void cleanup() noexcept {
            // TODO: destroy only???
            for (auto& write_set_value : write_set)
                write_set_value.dest_var().destroy_deallocate(write_set_value.pending_write());
            write_set.clear();
            read_set.clear();
        }
        
        read_set_const_iter read_set_find(const detail::var_base& src_var) const noexcept {
            return std::find_if(
                    std::begin(read_set),
                    std::end(read_set),
                    [&src_var](const detail::read_set_value_type& rhs) noexcept -> bool
                    { return rhs.is_src_var(src_var); });
        }

        void add_read_set(const detail::var_base& src_var) override final {
            if (read_set_find(src_var) == std::end(read_set))
                read_set.emplace_back(src_var);
        }

        void add_write_set(detail::var_base& dest_var,
                           detail::var_storage pending_write) override final {
            // up to caller to ensure dest_var is not already in the write_set
            assert(!find_write_set(dest_var).success());
            write_set.emplace_back(dest_var, std::move(pending_write));
        }

        detail::write_set_lookup
        find_write_set(const detail::var_base& dest_var) noexcept override final {
            const auto end = std::end(write_set);
            const auto iter = std::find_if(
                std::begin(write_set),
                end,
                [&dest_var](const detail::write_set_value_type& rhs) noexcept -> bool
                { return rhs.is_dest_var(dest_var); });
            return iter != end
                ? detail::write_set_lookup{iter->pending_write()}
                : detail::write_set_lookup{nullptr};
        }
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

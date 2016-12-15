#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/small_pod_hash_set.hpp>
#include <lstm/detail/thread_data.hpp>

#include <lstm/transaction_domain.hpp>

#include <atomic>
#include <cassert>

LSTM_DETAIL_BEGIN
    template<typename T, typename Alloc>
    struct deleter {
    private:
        T* ptr;
        Alloc* alloc;
        
        using alloc_traits = std::allocator_traits<Alloc>;
        
    public:
        deleter() noexcept = default;
        
        deleter(T* in_ptr, Alloc* in_alloc) noexcept
            : ptr(in_ptr)
            , alloc(in_alloc)
        { assert(ptr && alloc); }
        
        void operator()() const noexcept {
            alloc_traits::destroy(*alloc, ptr);
            alloc_traits::deallocate(*alloc, ptr, 1);
        }
    };
    
    template<typename T>
    struct deleter<T, void> {
    private:
        T* ptr;
        
    public:
        deleter() noexcept = default;
        
        deleter(T* in_ptr, void*) noexcept
            : ptr(in_ptr)
        { assert(ptr); }
        
        void operator()() const noexcept { delete ptr; }
    };
    
    struct write_set_lookup {
        var_storage* pending_write_;
        hash_t hash;
        
        inline constexpr bool success() const noexcept { return !hash; }
        inline constexpr var_storage& pending_write() const noexcept { return *pending_write_; }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    [[noreturn]] inline void retry() {
        LSTM_USER_FAIL_TX();
        throw detail::_tx_retry{};
    }

    struct transaction {
    protected:
        friend detail::atomic_fn;
        friend test::transaction_tester;
        
        transaction_domain* _domain;
        detail::thread_data* tls_td;
        word read_version;
        
        inline transaction_domain& domain() noexcept { return *_domain; }
        inline const transaction_domain& domain() const noexcept { return *_domain; }
        
        inline void reset_read_version() noexcept { read_version = domain().get_clock(); }

        inline transaction(transaction_domain& in_domain, detail::thread_data& in_tls_td) noexcept
            : _domain(&in_domain)
            , tls_td(&in_tls_td)
        {
            reset_read_version();
            assert(&detail::tls_thread_data() == tls_td);
            assert(tls_td->tx == nullptr);
        }
        
    #ifndef NDEBUG
        inline ~transaction() noexcept { assert(tls_td->tx == nullptr); }
    #endif
        
        static inline bool locked(word version) noexcept { return version & 1; }
        static inline word as_locked(word version) noexcept { return version | 1; }

        bool lock(const detail::var_base& v) const noexcept {
            word version_buf = v.version_lock.load(LSTM_RELAXED);
            // TODO: not convinced of this ordering
            return version_buf <= read_version && !locked(version_buf) &&
                    v.version_lock.compare_exchange_strong(version_buf,
                                                           as_locked(version_buf),
                                                           LSTM_RELEASE);
        }

        static inline
        void unlock(const word version_to_set, const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));
            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }

        static inline void unlock(const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            v.version_lock.fetch_sub(1, LSTM_RELEASE);
        }

        inline bool read_valid(const detail::var_base& v) const noexcept
        { return v.version_lock.load(LSTM_ACQUIRE) <= read_version; }

        virtual void add_read_set(const detail::var_base& src_var) = 0;
        virtual void add_write_set(detail::var_base& dest_var,
                                   detail::var_storage pending_write,
                                   const detail::hash_t hash) = 0;
        
        virtual detail::write_set_lookup
        find_write_set(const detail::var_base& dest_var) noexcept = 0;

    public:
        // transactions can only be passed by non-const lvalue reference
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(!var<T, Alloc0>::atomic)>
        const T& load(const var<T, Alloc0>& src_var) {
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const word src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T& result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (src_version <= read_version
                        && !locked(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
            } else if (src_var.version_lock.load(LSTM_RELAXED) <= read_version)
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic)>
        T load(const var<T, Alloc0>& src_var) {
             detail::write_set_lookup lookup = find_write_set(src_var);
             if (LSTM_LIKELY(!lookup.success())) {
                const word src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (src_version <= read_version
                        && !locked(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
             } else if (src_var.version_lock.load(LSTM_RELAXED) <= read_version)
                 return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0, typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void store(var<T, Alloc0>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (LSTM_LIKELY(!lookup.success()))
                add_write_set(dest_var, dest_var.allocate_construct((U&&)u), lookup.hash);
            else
                var<T>::store(lookup.pending_write(), (U&&)u);
            
            if (!read_valid(dest_var)) detail::internal_retry();
        }

        template<typename T, typename Alloc = void>
        void delete_(T* dest_var, Alloc* alloc = nullptr)
        { tls_td->gp_callbacks.emplace_back(detail::deleter<T, Alloc>(dest_var, alloc)); }

        // reading/writing an rvalue probably never makes sense
        template<typename T>
        void load(const var<T>&& v) = delete;

        template<typename T>
        void store(var<T>&& v) = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

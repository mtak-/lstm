#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>
#include <lstm/detail/pod_hash_set.hpp>

#include <lstm/thread_data.hpp>
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
        
        static constexpr gp_t lock_bit = gp_t(1) << (sizeof(gp_t) * 8 - 1);
        
        transaction_domain* _domain;
        thread_data* tls_td;
        gp_t read_version;
        
        inline transaction_domain& domain() noexcept { return *_domain; }
        inline const transaction_domain& domain() const noexcept { return *_domain; }
        
        inline void reset_read_version() noexcept
        { tls_td->access_relock(read_version = domain().get_clock()); }
        
        inline transaction(transaction_domain& in_domain, thread_data& in_tls_td) noexcept
            : _domain(&in_domain)
            , tls_td(&in_tls_td)
        {
            assert(&tls_thread_data() == tls_td);
            assert(tls_td->tx == nullptr);
            assert(tls_td->active.load(LSTM_RELAXED) == detail::off_state);
            
            tls_td->access_lock(read_version = domain().get_clock());
        }
        
    #ifndef NDEBUG
        inline ~transaction() noexcept { assert(tls_td->tx == nullptr); }
    #endif
        
        static inline bool locked(const gp_t version) noexcept { return version & lock_bit; }
        static inline gp_t as_locked(const gp_t version) noexcept { return version | lock_bit; }
        
        inline bool rw_valid(const gp_t version) const noexcept
        { return version <= read_version; }
        
        inline bool rw_valid(const detail::var_base& v) const noexcept
        { return rw_valid(v.version_lock.load(LSTM_RELAXED)); }
        
        bool lock(const detail::var_base& v) const noexcept {
            gp_t version_buf = v.version_lock.load(LSTM_RELAXED);
            // TODO: not convinced of this ordering
            return rw_valid(version_buf) &&
                    v.version_lock.compare_exchange_strong(version_buf,
                                                           as_locked(version_buf),
                                                           LSTM_RELEASE);
        }
        
        static inline void unlock(const gp_t version_to_set, const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));
            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }
        
        static inline void unlock(const detail::var_base& v) noexcept {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            v.version_lock.fetch_xor(lock_bit, LSTM_RELEASE);
        }
        
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
            static_assert(std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
                      
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T& result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (rw_valid(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var))
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc0,
            LSTM_REQUIRES_(var<T, Alloc0>::atomic)>
        T load(const var<T, Alloc0>& src_var) {
            static_assert(!std::is_reference<decltype(var<T>::load(src_var.storage.load()))>{}, "");
            
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (LSTM_LIKELY(!lookup.success())) {
                const gp_t src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                const T result = var<T>::load(src_var.storage.load(LSTM_ACQUIRE));
                if (rw_valid(src_version)
                        && src_var.version_lock.load(LSTM_RELAXED) == src_version) {
                    add_read_set(src_var);
                    return result;
                }
            } else if (rw_valid(src_var))
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
            
            if (!rw_valid(dest_var)) detail::internal_retry();
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

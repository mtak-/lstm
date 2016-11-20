#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/detail/var_detail.hpp>
#include <lstm/transaction_domain.hpp>

#include <atomic>
#include <cassert>

LSTM_DETAIL_BEGIN
    struct deleter_base { virtual void operator()() noexcept = 0; };
    
    template<typename T, typename Alloc>
    struct deleter : deleter_base, private Alloc {
    private:
        T* ptr; // TODO: probly move this into base class?
        Alloc& alloc() noexcept { return *this; }
        
        using alloc_traits = std::allocator_traits<Alloc>;
        
    public:
        deleter(T* ptr_in, const Alloc& alloc) noexcept(std::is_nothrow_copy_constructible<Alloc>{})
            : Alloc(alloc)
            , ptr(ptr_in)
        {}
        
        void operator()() noexcept override final {
            alloc_traits::destroy(alloc(), ptr);
            alloc_traits::deallocate(alloc(), ptr, 1);
        }
    };
    
    using a_deleter_type = deleter<word, std::allocator<word>>;
    using deleter_storage = uninitialized<a_deleter_type>;
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
        word read_version;
        
        inline transaction_domain& domain() noexcept
        { return _domain == nullptr ? default_domain() : *_domain; }
        
        inline const transaction_domain& domain() const noexcept
        { return _domain == nullptr ? default_domain() : *_domain; }
        
        inline void reset_read_version() noexcept { read_version = domain().get_clock(); }

        inline transaction(transaction_domain* in_domain) noexcept
            : _domain(in_domain)
        { reset_read_version(); }
        
        static inline bool locked(word version) noexcept { return version & 1; }
        static inline word as_locked(word version) noexcept { return version | 1; }

        bool lock(word& version_buf, const detail::var_base& v) const noexcept {
            while (version_buf <= read_version && !locked(version_buf) &&
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

        virtual void add_read_set(const detail::var_base& src_var) = 0;
        virtual void add_write_set(detail::var_base& dest_var,
                                   detail::var_storage pending_write) = 0;
        
        virtual detail::write_set_lookup
        find_write_set(const detail::var_base& dest_var) noexcept = 0;
        
        virtual detail::deleter_storage& delete_set_push_back_storage() = 0;

    public:
        // transactions can only be passed by non-const lvalue reference
        transaction(const transaction&) = delete;
        transaction(transaction&&) = delete;
        transaction& operator=(const transaction&) = delete;
        transaction& operator=(transaction&&) = delete;
        
        template<typename T, typename Alloc,
            LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        const T& load(const var<T, Alloc>& src_var) {
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (!lookup.success()) {
                // TODO: this is not optimal!!!
                word src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                if (src_version <= read_version && !locked(src_version)) {
                    const T& result = var<T>::load(src_var.storage.load(LSTM_RELAXED));
                    if (src_var.version_lock.load(LSTM_RELEASE) == src_version) {
                        add_read_set(src_var);
                        return result;
                    }
                }
            } else if (read_valid(src_var))
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }
        
        template<typename T, typename Alloc,
            LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        T load(const var<T, Alloc>& src_var) {
            detail::write_set_lookup lookup = find_write_set(src_var);
            if (!lookup.success()) {
                // TODO: this is not optimal!!!
                word src_version = src_var.version_lock.load(LSTM_ACQUIRE);
                if (src_version <= read_version && !locked(src_version)) {
                    T result = var<T>::load(src_var.storage.load(LSTM_RELAXED));
                    if (src_var.version_lock.load(LSTM_RELEASE) == src_version) {
                        add_read_set(src_var);
                        return result;
                    }
                }
            } else if (read_valid(src_var))
                return var<T>::load(lookup.pending_write());
            detail::internal_retry();
        }

        template<typename T, typename Alloc, typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, U&&>() &&
                           std::is_constructible<T, U&&>())>
        void store(var<T, Alloc>& dest_var, U&& u) {
            detail::write_set_lookup lookup = find_write_set(dest_var);
            if (!lookup.success())
                add_write_set(dest_var, dest_var.allocate_construct((U&&)u));
            else
                dest_var.store(lookup.pending_write(), (U&&)u);

            // TODO: where is this best placed???, or is it best removed altogether?
            if (!read_valid(dest_var)) detail::internal_retry();
        }

        template<typename T, typename Alloc = std::allocator<T>>
        void delete_(T* dest_var, const Alloc& alloc = {}) {
            static_assert(sizeof(detail::deleter<T, Alloc>) == sizeof(detail::deleter_storage));
            static_assert(alignof(detail::deleter<T, Alloc>) == alignof(detail::deleter_storage));
            static_assert(std::is_trivially_destructible<detail::deleter<T, Alloc>>{});
            
            detail::deleter_storage& storage = delete_set_push_back_storage();
            ::new (&storage) detail::deleter<T, Alloc>(dest_var, alloc);
        }

        // TODO: reading/writing an rvalue probably never makes sense?
        template<typename T>
        void load(const var<T>&& v) = delete;

        template<typename T>
        void store(var<T>&& v) = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

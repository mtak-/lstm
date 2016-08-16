#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_DETAIL_BEGIN
    struct var_base {
    protected:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        friend test::transaction_tester;
        
        inline var_base() noexcept
            : version_lock{0}
        {}
        
        virtual void destroy_deallocate(void* ptr) noexcept = 0;
        
        mutable std::atomic<word> version_lock;
        void* value;
    };
    
    enum class var_type {
        value,
        atomic,
        lvalue_ref
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        if (std::is_lvalue_reference<T>{}) return var_type::lvalue_ref;
        
        if (sizeof(T) <= sizeof(word) && alignof(T) <= alignof(word) &&
                std::is_trivially_copyable<T>{}())
            return var_type::atomic;
        
        return var_type::value;
    }
    
    template<typename T, typename Alloc, var_type = var_type_switch<T>()>
    struct var_alloc_policy;
    
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::value>
        : private Alloc
        , var_base
    {
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        using alloc_traits = std::allocator_traits<Alloc>;
        constexpr Alloc& alloc() noexcept { return static_cast<Alloc&>(*this); }
        
        constexpr var_alloc_policy()
            noexcept(std::is_nothrow_default_constructible<Alloc>{}) = default;
        constexpr var_alloc_policy(const Alloc& alloc)
            noexcept(std::is_nothrow_constructible<Alloc, const Alloc&>{})
            : Alloc(alloc)
        {}
        
        template<typename... Us>
        constexpr void* allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us&&)us...)))
        {
            T* ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), ptr, (Us&&)us...);
            return ptr;
        }
        
        void destroy_deallocate(void* void_ptr) noexcept override final {
            auto ptr = &load(void_ptr);
            alloc_traits::destroy(alloc(), ptr);
            alloc_traits::deallocate(alloc(), ptr, 1);
        }
        
        static T& load(void*& v) noexcept { return *static_cast<T*>(v); }
        static const T& load(void* const& v)  noexcept { return *static_cast<const T*>(v); }
    };
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::atomic>
        : var_base
    {
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        
        constexpr var_alloc_policy() noexcept = default;
        constexpr var_alloc_policy(const Alloc&) noexcept {}
        
        template<typename... Us>
        constexpr void* allocate_construct(Us&&... us) noexcept(noexcept(T((Us&&)us...)))
        { return reinterpret_cast<void*>(T((Us&&)us...)); }
        
        void destroy_deallocate(void* void_ptr) noexcept override final
        { load(void_ptr).~T(); }
        
        static T& load(void*& v) noexcept { return reinterpret_cast<T&>(v); }
        static const T& load(void* const& v) noexcept
        { return reinterpret_cast<const T&>(v); }
    };
    
    template<typename T, typename Alloc>
    struct var_impl
        : var_alloc_policy<T, Alloc>
    {
    private:
        using base = var_alloc_policy<T, Alloc>;
        
    public:
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>() &&
                           !std::is_same<uncvref<U>, uncvref<Alloc>>())>
        constexpr var_impl(U&& u, Us&&... us)
            noexcept(noexcept(base::allocate_construct((U&&)u, (Us&&)us...)))
        { var_base::value = (void*)base::allocate_construct((U&&)u, (Us&&)us...); }
        
        LSTM_REQUIRES(std::is_constructible<T>())
        constexpr var_impl() noexcept(noexcept(base::allocate_construct()))
        { var_base::value = (void*)base::allocate_construct(); }
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var_impl(const Alloc& in_alloc, Us&&... us)
            noexcept(noexcept(base::allocate_construct((Us&&)us...)))
            : var_alloc_policy<T, Alloc>{in_alloc}
        { var_base::value = (void*)base::allocate_construct((Us&&)us...); }
        
        T& unsafe() & noexcept { return base::load(var_base::value); }
        T&& unsafe() && noexcept { return std::move(base::load(var_base::value)); }
        const T& unsafe() const & noexcept { return base::load(var_base::value); }
        const T&& unsafe() const && noexcept { return std::move(base::load(var_base::value)); }
        
        ~var_impl() noexcept { base::destroy_deallocate(var_base::value); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
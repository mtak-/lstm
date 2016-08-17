#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_BEGIN
    // slowest to fastest
    enum class var_type {
        locking,
        trivial,
        atomic,
    };
LSTM_END

LSTM_DETAIL_BEGIN
    using var_storage = void*;

    struct var_base {
    protected:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        friend test::transaction_tester;
        
        inline var_base() noexcept : version_lock{0} {}
        
        virtual void destroy_deallocate(var_storage ptr) noexcept = 0;
        
        mutable std::atomic<word> version_lock;
        var_storage value;
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        if (sizeof(T) <= sizeof(word) && alignof(T) <= alignof(word) &&
                std::is_trivially_copyable<T>{}())
            return var_type::atomic;
        
        return var_type::locking;
    }
    
    template<typename T, typename Alloc, var_type = var_type_switch<T>()>
    struct var_alloc_policy;
    
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::locking>
        : private Alloc
        , var_base
    {
        static constexpr bool trivial = std::is_trivially_copyable<T>{}();
        static constexpr bool atomic = false;
        static constexpr var_type type = trivial ? var_type::locking : var_type::trivial;
    protected:
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
        constexpr var_storage allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us&&)us...)))
        {
            T* ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), ptr, (Us&&)us...);
            return ptr;
        }
        
        void destroy_deallocate(var_storage s) noexcept override final {
            auto ptr = &load(s);
            alloc_traits::destroy(alloc(), ptr);
            alloc_traits::deallocate(alloc(), ptr, 1);
        }
        
        static T& load(var_storage& v) noexcept { return *static_cast<T*>(v); }
        static const T& load(const var_storage& v)  noexcept { return *static_cast<const T*>(v); }
    };
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::atomic>
        : var_base
    {
        static constexpr bool trivial = true;
        static constexpr bool atomic = true;
        static constexpr var_type type = var_type::atomic;
    protected:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        
        constexpr var_alloc_policy() noexcept = default;
        constexpr var_alloc_policy(const Alloc&) noexcept {}
        
        template<typename... Us>
        constexpr var_storage allocate_construct(Us&&... us) noexcept(noexcept(T((Us&&)us...)))
        { return reinterpret_cast<var_storage>(T((Us&&)us...)); }
        
        void destroy_deallocate(var_storage s) noexcept override final
        { load(s).~T(); }
        
        static T& load(var_storage& v) noexcept { return reinterpret_cast<T&>(v); }
        static const T& load(const var_storage& v) noexcept
        { return reinterpret_cast<const T&>(v); }
    };
    
    
    /***********************************************************************************************
     * var_impl
     **********************************************************************************************/
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
        { var_base::value = base::allocate_construct((U&&)u, (Us&&)us...); }
        
        LSTM_REQUIRES(std::is_constructible<T>())
        constexpr var_impl() noexcept(noexcept(base::allocate_construct()))
        { var_base::value = base::allocate_construct(); }
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var_impl(const Alloc& in_alloc, Us&&... us)
            noexcept(noexcept(base::allocate_construct((Us&&)us...)))
            : var_alloc_policy<T, Alloc>{in_alloc}
        { var_base::value = base::allocate_construct((Us&&)us...); }
        
        T& unsafe() & noexcept { return base::load(var_base::value); }
        T&& unsafe() && noexcept { return std::move(base::load(var_base::value)); }
        const T& unsafe() const & noexcept { return base::load(var_base::value); }
        const T&& unsafe() const && noexcept { return std::move(base::load(var_base::value)); }
        
        ~var_impl() noexcept { base::destroy_deallocate(var_base::value); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
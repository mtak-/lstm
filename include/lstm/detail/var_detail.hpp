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
    struct var_base {
    protected:
        mutable std::atomic<word> version_lock;
        var_storage storage;
        var_type kind;
        
        inline var_base(const var_type in_kind) noexcept
            : version_lock{0}
            , kind{in_kind}
        {}
        
        virtual ~var_base() noexcept = default;
        
        virtual void destroy_deallocate(var_storage storage) noexcept = 0;
        
        template<typename> friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::transaction;
        friend test::transaction_tester;
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        if (sizeof(T) <= sizeof(word) && alignof(T) <= alignof(word) &&
                std::is_trivially_copyable<T>{}())
            return var_type::atomic;
        else if (std::is_trivially_copyable<T>{}())
            return var_type::trivial;
        return var_type::locking;
    }
    
    template<typename T, typename Alloc, var_type Var_type = var_type_switch<T>()>
    struct var_alloc_policy
        : private Alloc
        , var_base
    {
        static constexpr bool trivial = std::is_trivially_copyable<T>{}();
        static constexpr bool atomic = false;
        static constexpr var_type type = Var_type;
    protected:
        template<typename> friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::transaction;
        using alloc_traits = std::allocator_traits<Alloc>;
        constexpr Alloc& alloc() noexcept { return static_cast<Alloc&>(*this); }
        
        constexpr var_alloc_policy()
            noexcept(std::is_nothrow_default_constructible<Alloc>{})
            : Alloc()
            , var_base{type}
        {}
        
        constexpr var_alloc_policy(const Alloc& alloc)
            noexcept(std::is_nothrow_constructible<Alloc, const Alloc&>{})
            : Alloc(alloc)
            , var_base{type}
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
        
        static T& load(var_storage& storage) noexcept { return *static_cast<T*>(storage); }
        static const T& load(const var_storage& storage)  noexcept
        { return *static_cast<const T*>(storage); }
    };
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::atomic>
        : var_base
    {
        static constexpr bool trivial = true;
        static constexpr bool atomic = true;
        static constexpr var_type type = var_type::atomic;
    protected:
        template<typename> friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::transaction;
        
        constexpr var_alloc_policy() noexcept : var_base{type} {}
        constexpr var_alloc_policy(const Alloc&) noexcept : var_base{type} {}
        
        template<typename... Us>
        var_storage allocate_construct(Us&&... us)
            noexcept(std::is_nothrow_constructible<T, Us&&...>{})
        { return reinterpret_cast<var_storage>(T((Us&&)us...)); }
        
        void destroy_deallocate(var_storage s) noexcept override final
        { load(s).~T(); }
        
        static T& load(var_storage& storage) noexcept { return reinterpret_cast<T&>(storage); }
        static const T& load(const var_storage& storage) noexcept
        { return reinterpret_cast<const T&>(storage); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
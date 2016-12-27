#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_BEGIN
    // TODO: this naming is wrong now. nothing is locking except in the commit phase
    enum class var_type {
        locking,
        trivial,
        atomic,
    };
LSTM_END

LSTM_DETAIL_BEGIN
    struct LSTM_CACHE_ALIGNED var_base {
    protected:
        mutable std::atomic<gp_t> version_lock;
        std::atomic<var_storage> storage;
        
        inline var_base() noexcept : version_lock{0} {}
        
        template<typename, std::size_t, std::size_t, std::size_t>
        friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::detail::write_set_deleter;
        friend struct ::lstm::transaction;
        friend test::transaction_tester;
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        if (sizeof(T) <= sizeof(word) && alignof(T) <= alignof(word) &&
                std::is_trivially_copy_constructible<T>{}() &&
                std::is_trivially_move_constructible<T>{}() &&
                std::is_trivially_destructible<T>{}())
            return var_type::atomic;
        else if (std::is_trivially_copy_constructible<T>{}() &&
                 std::is_trivially_move_constructible<T>{}() &&
                 std::is_trivially_destructible<T>{}())
            return var_type::trivial;
        return var_type::locking;
    }
    
    template<typename T, typename Alloc, var_type Var_type = var_type_switch<T>()>
    struct var_alloc_policy
        : private Alloc
        , var_base
    {
        static constexpr bool trivial = std::is_trivially_copy_constructible<T>{}();
        static constexpr bool atomic = false;
        static constexpr var_type type = Var_type;
    protected:
        template<typename, std::size_t, std::size_t, std::size_t>
        friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::transaction;
        using alloc_traits = std::allocator_traits<Alloc>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
            "sorry, lstm only supports allocators that return raw pointers");
        
        constexpr Alloc& alloc() noexcept { return static_cast<Alloc&>(*this); }
        
        constexpr var_alloc_policy()
            noexcept(std::is_nothrow_default_constructible<Alloc>{})
            : Alloc()
        {}
        
        constexpr var_alloc_policy(const Alloc& alloc)
            noexcept(std::is_nothrow_constructible<Alloc, const Alloc&>{})
            : Alloc(alloc)
        {}
        
        ~var_alloc_policy() noexcept
        { var_alloc_policy::destroy_deallocate(storage.load(LSTM_RELAXED)); }
        
        template<typename... Us>
        constexpr var_storage allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us&&)us...)))
        {
            auto ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), to_raw_pointer(ptr), (Us&&)us...);
            return ptr;
        }
        
        void destroy_deallocate(var_storage s) noexcept {
            T* ptr = &load(s);
            alloc_traits::destroy(alloc(), ptr);
            alloc_traits::deallocate(alloc(), ptr, 1);
        }
        
        static T& load(var_storage storage) noexcept { return *static_cast<T*>(storage); }
        
        template<typename U>
        static void store(var_storage storage, U&& u)
            noexcept(std::is_nothrow_assignable<T&, U&&>{})
        { load(storage) = (U&&)u; }
        
        template<typename U>
        static void store(std::atomic<var_storage>& storage, U&& u)
            noexcept(std::is_nothrow_assignable<T&, U&&>{})
        { store(storage.load(LSTM_RELAXED), (U&&)u); }
        
        template<typename U>
        static void store(const std::atomic<var_storage>& storage, U&&) noexcept = delete;
        template<typename U>
        static void store(const std::atomic<var_storage>&& storage, U&&) noexcept = delete;
    };
    
    // TODO: extremely likely there's strict aliasing issues...
    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::atomic>
        : var_base
    {
        static constexpr bool trivial = true;
        static constexpr bool atomic = true;
        static constexpr var_type type = var_type::atomic;
    protected:
        template<typename, std::size_t, std::size_t, std::size_t>
        friend struct ::lstm::detail::transaction_impl;
        friend struct ::lstm::transaction;
        
        constexpr var_alloc_policy() noexcept = default;
        constexpr var_alloc_policy(const Alloc&) noexcept {};
        
        ~var_alloc_policy() noexcept { load(storage.load(LSTM_RELAXED)).~T(); }
        
        template<typename... Us>
        var_storage allocate_construct(Us&&... us)
            noexcept(std::is_nothrow_constructible<T, Us&&...>{})
        { return reinterpret_cast<var_storage>(T((Us&&)us...)); }
        
        static T load(var_storage storage) noexcept { return reinterpret_cast<T&>(storage); }
        
        template<typename U>
        static void store(var_storage& storage, U&& u) noexcept
        { reinterpret_cast<T&>(storage) = (U&&)u; }
        
        template<typename U>
        static void store(std::atomic<var_storage>& storage, U&& u) noexcept
        { storage.store(reinterpret_cast<var_storage>(static_cast<T>(u)), LSTM_RELAXED); }
        
        template<typename U>
        static void store(const std::atomic<var_storage>& storage, U&&) noexcept = delete;
        template<typename U>
        static void store(const std::atomic<var_storage>&& storage, U&&) noexcept = delete;
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_VAR_HPP */

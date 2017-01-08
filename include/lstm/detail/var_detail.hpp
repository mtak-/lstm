#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_BEGIN
    enum class var_type {
        heap,
        atomic,
    };
LSTM_END

LSTM_DETAIL_BEGIN
    struct var_base {
    protected:
        mutable std::atomic<gp_t> version_lock;
        std::atomic<var_storage> storage;
        
        inline var_base() noexcept : version_lock{0} {}
        
        friend struct ::lstm::transaction;
        friend test::transaction_tester;
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        if (sizeof(T) <= sizeof(word) &&
                alignof(T) <= alignof(word) &&
                std::is_trivially_copy_constructible<T>{}() &&
                std::is_trivially_move_constructible<T>{}() &&
                std::is_trivially_copy_assignable<T>{}() &&
                std::is_trivially_move_assignable<T>{}() &&
                std::is_trivially_destructible<T>{}())
            return var_type::atomic;
        return var_type::heap;
    }
    
    // TODO: uses allocator constructors?
    
    template<typename T, typename Alloc, var_type Var_type = var_type_switch<T>()>
    struct var_alloc_policy
        : private Alloc
        , var_base
    {
        static constexpr bool heap = true;
        static constexpr bool atomic = false;
        static constexpr var_type type = Var_type;
    protected:
        friend struct ::lstm::transaction;
        using alloc_traits = std::allocator_traits<Alloc>;
        
        constexpr Alloc& alloc() noexcept { return *this; }
        constexpr const Alloc& alloc() const noexcept { return *this; }
        
        using Alloc::Alloc;
        
        ~var_alloc_policy() noexcept
        { var_alloc_policy::destroy_deallocate(alloc(), storage.load(LSTM_RELAXED)); }
        
        template<typename... Us>
        var_storage allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us&&)us...)))
        {
            T* ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), ptr, (Us&&)us...);
            return ptr;
        }
        
        static void destroy_deallocate(Alloc& alloc, var_storage s) noexcept {
            T* ptr = &load(s);
            alloc_traits::destroy(alloc, ptr);
            alloc_traits::deallocate(alloc, ptr, 1);
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
        : private Alloc
        , var_base
    {
        static constexpr bool heap = false;
        static constexpr bool atomic = true;
        static constexpr var_type type = var_type::atomic;
    protected:
        friend struct ::lstm::transaction;
        using alloc_traits = std::allocator_traits<Alloc>;
        
        constexpr Alloc& alloc() noexcept { return *this; }
        constexpr const Alloc& alloc() const noexcept { return *this; }
        
        using Alloc::Alloc;
        
        template<typename... Us>
        var_storage allocate_construct(Us&&... us)
            noexcept(std::is_nothrow_constructible<T, Us&&...>{})
        {
            var_storage result;
            alloc_traits::construct(alloc(), reinterpret_cast<T*>(&result), (Us&&)us...);
            return result;
        }
        
        static T load(var_storage storage) noexcept { return *reinterpret_cast<T*>(&storage); }
        
        template<typename U>
        static void store(var_storage& storage, U&& u) noexcept
        { reinterpret_cast<T&>(storage) = (U&&)u; }
        
        template<typename U>
        static void store(std::atomic<var_storage>& storage, U&& u) noexcept
        { storage.store(reinterpret_cast<var_storage>(static_cast<T>((U&&)u)), LSTM_RELAXED); }
        
        template<typename U>
        static void store(const std::atomic<var_storage>& storage, U&&) noexcept = delete;
        template<typename U>
        static void store(const std::atomic<var_storage>&& storage, U&&) noexcept = delete;
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_VAR_HPP */

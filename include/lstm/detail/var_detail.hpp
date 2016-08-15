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
        
        inline var_base(void* in_value) noexcept
            : version_lock{0}
            , value(in_value)
        {}
        
        virtual void destroy_deallocate(void* ptr) noexcept = 0;
        
        mutable std::atomic<word> version_lock;
        void* value;
    };
    
    enum class var_type {
        value,
        word_sized,
        lvalue_ref
    };
    
    template<typename T>
    constexpr var_type var_type_switch() noexcept {
        return std::is_lvalue_reference<T>{}
            ? var_type::lvalue_ref
            : sizeof(T) <= sizeof(word) && alignof(T) <= alignof(word) &&
                std::is_trivially_copyable<T>{}()
                    ? var_type::word_sized
                    : var_type::value;
    }
    
    template<typename T, typename Alloc, var_type = var_type_switch<T>()>
    struct var_impl
        : private Alloc
        , var_base
    {
    private:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        using alloc_traits = std::allocator_traits<Alloc>;
        constexpr Alloc& alloc() noexcept { return static_cast<Alloc&>(*this); }
        
    public:
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>() &&
                           !std::is_same<uncvref<U>, uncvref<Alloc>>())>
        constexpr var_impl(U&& u, Us&&... us)
            noexcept(noexcept(allocate_construct((U&&)u, (Us&&)us...)))
            : var_base{(void*)allocate_construct((U&&)u, (Us&&)us...)}
        {}
        
        LSTM_REQUIRES(std::is_constructible<T>())
        constexpr var_impl() noexcept(noexcept(allocate_construct()))
            : var_base{(void*)allocate_construct()}
        {}
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var_impl(const Alloc& in_alloc, Us&&... us)
            noexcept(noexcept(allocate_construct((Us&&)us...)))
            : Alloc{in_alloc}
            , var_base{(void*)allocate_construct((Us&&)us...)}
        {}
        
        T& unsafe() & noexcept { return *(T*)value; }
        T&& unsafe() && noexcept { return std::move(*(T*)value); }
        const T& unsafe() const & noexcept { return *(T*)value; }
        const T&& unsafe() const && noexcept { return std::move(*(T*)value); }
        
        ~var_impl() noexcept { destroy_deallocate(value); }
    
    private:
        template<typename... Us>
        constexpr T* allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us&&)us...)))
        {
            T* ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), ptr, (Us&&)us...);
            return ptr;
        }
        
        void destroy_deallocate(void* void_ptr) noexcept override final {
            auto ptr = static_cast<T*>(void_ptr);
            alloc_traits::destroy(alloc(), ptr);
            alloc_traits::deallocate(alloc(), ptr, 1);
        }
    };
    
    template<typename T, typename Alloc>
    struct var_impl<T, Alloc, var_type::word_sized>
        : var_base
    {
    private:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        
    public:
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>() &&
                           !std::is_same<uncvref<U>, uncvref<Alloc>>())>
        constexpr var_impl(U&& u, Us&&... us)
            noexcept(noexcept(T((U&&)u, (Us&&)us...)))
            : var_base{reinterpret_cast<void*>(T((U&&)u, (Us&&)us...))}
        {}
        
        LSTM_REQUIRES(std::is_constructible<T>())
        constexpr var_impl() noexcept(noexcept(T()))
            : var_base{reinterpret_cast<void*>(T())}
        {}
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var_impl(const Alloc&, Us&&... us)
            noexcept(noexcept(T((Us&&)us...)))
            : var_base{reinterpret_cast<void*>(T((Us&&)us...))}
        {}
        
        T& unsafe() & noexcept { return reinterpret_cast<T&>(value); }
        T&& unsafe() && noexcept { return reinterpret_cast<T&&>(value); }
        const T& unsafe() const & noexcept { return reinterpret_cast<const T&>(value); }
        const T&& unsafe() const && noexcept { return reinterpret_cast<const T&&>(value); }
        
        ~var_impl() noexcept { destroy_deallocate(value); }
    
    private:
        template<typename... Us>
        constexpr void* allocate_construct(Us&&... us) noexcept(noexcept(T((Us&&)us...)))
        { return reinterpret_cast<void*>(T((Us&&)us...)); }
        
        void destroy_deallocate(void* void_ptr) noexcept override final
        { reinterpret_cast<T&>(void_ptr).~T(); }
    };

    // TODO: this one is super broke
    template<typename T, typename Alloc>
    struct var_impl<T, Alloc, var_type::lvalue_ref>
        : private Alloc
        , var_base
    {
    private:
        template<typename> friend struct ::lstm::transaction;
        friend struct ::lstm::detail::transaction_base;
        using elem_type = uncvref<T>;
        using alloc_traits = std::allocator_traits<Alloc>;
        constexpr Alloc& alloc() noexcept { return static_cast<Alloc&>(*this); }
        
    public:
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<T, U&>())>
        constexpr var_impl(U& u) noexcept : var_base{(void*)std::addressof(u)} {}
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<T, U&>())>
        constexpr var_impl(const Alloc& in_alloc, U& u) noexcept
            : Alloc(in_alloc)
            , var_base{(void*)std::addressof(u)}
        {}
        
        T unsafe() const noexcept { return static_cast<T>(*(elem_type*)value); }
        
        ~var_impl() noexcept {}
    
    private:
        template<typename... Us>
        constexpr elem_type* allocate_construct(Us&&... us)
            noexcept(noexcept(alloc_traits::allocate(alloc(), 1)) &&
                     noexcept(alloc_traits::construct(alloc(), (elem_type*)nullptr, (Us&&)us...)))
        {
            elem_type* ptr = alloc_traits::allocate(alloc(), 1);
            alloc_traits::construct(alloc(), ptr, (Us&&)us...);
            return ptr;
        }
        
        void destroy_deallocate(void*) noexcept override final {}
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
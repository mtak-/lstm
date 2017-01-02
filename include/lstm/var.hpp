#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var : private detail::var_alloc_policy<T, Alloc> {
    private:
        using base = detail::var_alloc_policy<T, Alloc>;
        static_assert(std::is_same<Alloc, detail::uncvref<Alloc>>{},
            "lstm::var<> allocators cannot be cv/ref qualified!");
        static_assert(!std::is_reference<T>{},
            "lstm::var<>'s cannot contain a reference");
        static_assert(std::is_same<detail::uncvref<T>, typename Alloc::value_type>{},
            "lstm::var<> given invalid allocator for type T");
        static_assert(!std::is_const<typename Alloc::value_type>{} &&
                      !std::is_volatile<typename Alloc::value_type>{},
            "lstm::var<> does not currently support cv qualifications on T");
        static_assert(!std::is_array<T>{},
            "lstm::var<> does not support raw c arrays. try using a std::array");
        
        friend struct ::lstm::transaction;
        friend test::transaction_tester;
        
    public:
        using base::heap;
        using base::atomic;
        using base::type;
        
        // TODO: support construction from initializer_lists
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>() &&
                           !std::is_same<detail::uncvref<U>, Alloc>())>
        constexpr var(U&& u, Us&&... us)
            noexcept(noexcept(base::allocate_construct((U&&)u, (Us&&)us...)))
        { detail::var_base::storage.store(base::allocate_construct((U&&)u, (Us&&)us...),
                                          LSTM_RELAXED); }
        
        LSTM_REQUIRES(std::is_default_constructible<T>())
        constexpr var() noexcept(noexcept(base::allocate_construct()))
        { detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED); }
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var(const Alloc& in_alloc, Us&&... us)
            noexcept(noexcept(base::allocate_construct((Us&&)us...)))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct((Us&&)us...),
                                          LSTM_RELAXED); }
        
        T unsafe_read() const noexcept
        { return base::load(detail::var_base::storage.load(LSTM_RELAXED)); }
        
        void unsafe_write(const T& t) noexcept
        { return base::store(detail::var_base::storage, t); }
        
        void unsafe_write(T&& t) noexcept
        { return base::store(detail::var_base::storage, std::move(t)); }
    };
LSTM_END

#endif /* LSTM_VAR_HPP */

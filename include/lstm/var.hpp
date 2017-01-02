#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var : private detail::var_alloc_policy<T, Alloc> {
        using value_type = T;
        using allocator_type = Alloc;
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
        static_assert(std::is_pointer<typename std::allocator_traits<Alloc>::pointer>{},
            "sorry, lstm::var only supports allocators that return raw pointers");
        
        friend struct ::lstm::transaction;
        friend test::transaction_tester;
        
    public:
        using base::heap;
        using base::atomic;
        using base::type;
        
        LSTM_REQUIRES(std::is_default_constructible<T>{} &&
                      std::is_default_constructible<Alloc>{} &&
                      detail::implicit_default_constructor<T>{})
        constexpr var() noexcept(noexcept(base::allocate_construct()))
        { detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED); }
        
        LSTM_REQUIRES(std::is_default_constructible<T>{} &&
                      std::is_default_constructible<Alloc>{} &&
                      !detail::implicit_default_constructor<T>{})
        explicit constexpr var() noexcept(noexcept(base::allocate_construct()))
        { detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED); }
        
        LSTM_REQUIRES(std::is_default_constructible<T>{} &&
                      detail::implicit_default_constructor<T>{})
        constexpr var(std::allocator_arg_t, const Alloc& in_alloc)
            noexcept(noexcept(base::allocate_construct()))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED); }
        
        LSTM_REQUIRES(std::is_default_constructible<T>{} &&
                      !detail::implicit_default_constructor<T>{})
        explicit constexpr var(std::allocator_arg_t, const Alloc& in_alloc)
            noexcept(noexcept(base::allocate_construct()))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED); }
        
        template<typename Ilist, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Ilist&, Us&&...>{} &&
                           (std::is_convertible<Ilist&, T>{} || sizeof...(Us) > 0) &&
                           std::is_default_constructible<Alloc>{})>
        constexpr var(std::initializer_list<Ilist> is, Us&&... us)
            noexcept(noexcept(base::allocate_construct(is, (Us&&)us...)))
        { detail::var_base::storage.store(base::allocate_construct(is, (Us&&)us...),
                                          LSTM_RELAXED); }
        
        template<typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<T, Ilist&>{} &&
                           !std::is_convertible<Ilist&, T>{} &&
                           std::is_default_constructible<Alloc>{})>
        explicit constexpr var(std::initializer_list<Ilist> is)
            noexcept(noexcept(base::allocate_construct(is)))
        { detail::var_base::storage.store(base::allocate_construct(is), LSTM_RELAXED); }
        
        template<typename Ilist, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Ilist&, Us&&...>{} &&
                           (std::is_convertible<Ilist&, T>{} || sizeof...(Us) > 0))>
        constexpr var(std::allocator_arg_t,
                      const Alloc& in_alloc,
                      std::initializer_list<Ilist> is,
                      Us&&... us)
            noexcept(noexcept(base::allocate_construct(is, (Us&&)us...)))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct(is, (Us&&)us...),
                                          LSTM_RELAXED); }
        
        template<typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<T, Ilist&>{} &&
                           !std::is_convertible<Ilist&, T>{})>
        explicit constexpr var(std::allocator_arg_t,
                               const Alloc& in_alloc,
                               std::initializer_list<Ilist> is)
            noexcept(noexcept(base::allocate_construct(is)))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct(is), LSTM_RELAXED); }
        
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>{} &&
                           (std::is_convertible<U&&, T>{} || sizeof...(Us) > 0) &&
                           !std::is_same<detail::uncvref<U>, std::allocator_arg_t>{} &&
                           std::is_default_constructible<Alloc>{})>
        constexpr var(U&& u, Us&&... us)
            noexcept(noexcept(base::allocate_construct((U&&)u, (Us&&)us...)))
        { detail::var_base::storage.store(base::allocate_construct((U&&)u, (Us&&)us...),
                                          LSTM_RELAXED); }
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<T, U&&>{} &&
                           !std::is_convertible<U&&, T>{} &&
                           !std::is_same<detail::uncvref<U>, std::allocator_arg_t>{} &&
                           std::is_default_constructible<Alloc>{})>
        explicit constexpr var(U&& u)
            noexcept(noexcept(base::allocate_construct((U&&)u)))
        { detail::var_base::storage.store(base::allocate_construct((U&&)u), LSTM_RELAXED); }
        
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>{} &&
                           (std::is_convertible<U&&, T>{} || sizeof...(Us) > 0))>
        constexpr var(std::allocator_arg_t, const Alloc& in_alloc, U&& u, Us&&... us)
            noexcept(noexcept(base::allocate_construct((U&&)u, (Us&&)us...)))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct((U&&)u, (Us&&)us...),
                                          LSTM_RELAXED); }
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<T, U&&>{} &&
                           !std::is_convertible<U&&, T>{})>
        explicit constexpr var(std::allocator_arg_t, const Alloc& in_alloc, U&& u)
            noexcept(noexcept(base::allocate_construct((U&&)u)))
            : base{in_alloc}
        { detail::var_base::storage.store(base::allocate_construct((U&&)u), LSTM_RELAXED); }
        
        T unsafe_read() const noexcept
        { return base::load(detail::var_base::storage.load(LSTM_RELAXED)); }
        
        void unsafe_write(const T& t) noexcept
        { return base::store(detail::var_base::storage, t); }
        
        void unsafe_write(T&& t) noexcept
        { return base::store(detail::var_base::storage, std::move(t)); }
    };
LSTM_END

#endif /* LSTM_VAR_HPP */

#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var : private detail::var_alloc_policy<T, Alloc>
    {
        using value_type     = T;
        using allocator_type = Alloc;

    private:
        using base = detail::var_alloc_policy<value_type, allocator_type>;
        static_assert(std::is_same<allocator_type, detail::uncvref<allocator_type>>{},
                      "lstm::var<> allocators cannot be cv/ref qualified!");
        static_assert(!std::is_reference<value_type>{}, "lstm::var<>'s cannot contain a reference");
        static_assert(
            std::is_same<detail::uncvref<value_type>, typename allocator_type::value_type>{},
            "lstm::var<> given invalid allocator for value_type");
        static_assert(!std::is_const<typename allocator_type::value_type>{}
                          && !std::is_volatile<typename allocator_type::value_type>{},
                      "lstm::var<> does not currently support cv qualifications on value_type");
        static_assert(!std::is_array<value_type>{},
                      "lstm::var<> does not support raw c arrays. try using a std::array");
        static_assert(std::is_pointer<typename std::allocator_traits<allocator_type>::pointer>{},
                      "sorry, lstm::var only supports allocators that return raw pointers");

        friend struct ::lstm::transaction;
        friend test::transaction_tester;

    public:
        using base::heap;
        using base::atomic;
        using base::type;

        ///////////////////////////////////////////
        // DEFAULT CONSTRUCTORS
        ///////////////////////////////////////////
        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && std::is_default_constructible<allocator_type>{}
                      && detail::is_convertible<value_type>{})
        constexpr var() noexcept(noexcept(base::allocate_construct()))
        {
            detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && std::is_default_constructible<allocator_type>{}
                      && !detail::is_convertible<value_type>{})
        explicit constexpr var() noexcept(noexcept(base::allocate_construct()))
        {
            detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && detail::is_convertible<value_type>{})
        constexpr var(std::allocator_arg_t,
                      const allocator_type& in_alloc) noexcept(noexcept(base::allocate_construct()))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && !detail::is_convertible<value_type>{})
        explicit constexpr var(std::allocator_arg_t, const allocator_type& in_alloc) noexcept(
            noexcept(base::allocate_construct()))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(), LSTM_RELAXED);
        }

        ///////////////////////////////////////////
        // initializer_list CONSTRUCTORS
        ///////////////////////////////////////////
        template<typename Ilist,
                 typename... Us,
                 LSTM_REQUIRES_(
                     std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                     && detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                     && std::is_default_constructible<allocator_type>{}
                     && sizeof...(Us) > 0)>
        constexpr var(std::initializer_list<Ilist> is,
                      Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...)))
        {
            detail::var_base::storage.store(base::allocate_construct(is, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && !detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && std::is_default_constructible<allocator_type>{}
                && sizeof...(Us) > 0)>
        explicit constexpr var(std::initializer_list<Ilist> is, Us&&... us) noexcept(
            noexcept(base::allocate_construct(is, (Us &&) us...)))
        {
            detail::var_base::storage.store(base::allocate_construct(is, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        // support for var<std::vector<int>> = {1,2,3};
        template<
            typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<value_type, std::initializer_list<Ilist>&>{}
                           && detail::is_convertible<value_type, std::initializer_list<Ilist>&>{}
                           && std::is_default_constructible<allocator_type>{})>
        constexpr var(std::initializer_list<Ilist> is) noexcept(
            noexcept(base::allocate_construct(is)))
        {
            detail::var_base::storage.store(base::allocate_construct(is), LSTM_RELAXED);
        }

        template<
            typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<value_type, std::initializer_list<Ilist>&>{}
                           && !detail::is_convertible<value_type, std::initializer_list<Ilist>&>{}
                           && std::is_default_constructible<allocator_type>{})>
        explicit constexpr var(std::initializer_list<Ilist> is) noexcept(
            noexcept(base::allocate_construct(is)))
        {
            detail::var_base::storage.store(base::allocate_construct(is), LSTM_RELAXED);
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{})>
        constexpr var(std::allocator_arg_t,
                      const allocator_type&        in_alloc,
                      std::initializer_list<Ilist> is,
                      Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...)))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(is, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && !detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{})>
        explicit constexpr var(
            std::allocator_arg_t,
            const allocator_type&        in_alloc,
            std::initializer_list<Ilist> is,
            Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...)))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(is, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        ///////////////////////////////////////////
        // FORWARDING CONSTRUCTORS
        ///////////////////////////////////////////
        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && detail::is_convertible<value_type, U&&, Us&&...>{}
                                && !std::is_same<detail::uncvref<U>, std::allocator_arg_t>{}
                                && std::is_default_constructible<allocator_type>{})>
        constexpr var(U&& u, Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u,
                                                                                    (Us &&) us...)))
        {
            detail::var_base::storage.store(base::allocate_construct((U &&) u, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && !detail::is_convertible<value_type, U&&, Us&&...>{}
                                && !std::is_same<detail::uncvref<U>, std::allocator_arg_t>{}
                                && std::is_default_constructible<allocator_type>{})>
        explicit constexpr var(U&& u, Us&&... us) noexcept(
            noexcept(base::allocate_construct((U &&) u, (Us &&) us...)))
        {
            detail::var_base::storage.store(base::allocate_construct((U &&) u, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && detail::is_convertible<value_type, U&&, Us&&...>{})>
        constexpr var(std::allocator_arg_t,
                      const allocator_type& in_alloc,
                      U&&                   u,
                      Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u,
                                                                             (Us &&) us...)))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct((U &&) u, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && !detail::is_convertible<value_type, U&&, Us&&...>{})>
        explicit constexpr var(
            std::allocator_arg_t,
            const allocator_type& in_alloc,
            U&&                   u,
            Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u, (Us &&) us...)))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct((U &&) u, (Us &&) us...),
                                            LSTM_RELAXED);
        }

        ///////////////////////////////////////////
        // BRACED INIT LIST CONSTRUCTORS
        ///////////////////////////////////////////
        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && detail::is_convertible<value_type, value_type&&>{}
                      && std::is_default_constructible<allocator_type>{})
        constexpr var(value_type&& t) noexcept(noexcept(base::allocate_construct(std::move(t))))
        {
            detail::var_base::storage.store(base::allocate_construct(std::move(t)), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && !detail::is_convertible<value_type, value_type&&>{}
                      && std::is_default_constructible<allocator_type>{})
        explicit constexpr var(value_type&& t) noexcept(
            noexcept(base::allocate_construct(std::move(t))))
        {
            detail::var_base::storage.store(base::allocate_construct(std::move(t)), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && detail::is_convertible<value_type, value_type&&>{})
        constexpr var(std::allocator_arg_t,
                      const allocator_type& in_alloc,
                      value_type&& t) noexcept(noexcept(base::allocate_construct(std::move(t))))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(std::move(t)), LSTM_RELAXED);
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && !detail::is_convertible<value_type, value_type&&>{})
        explicit constexpr var(
            std::allocator_arg_t,
            const allocator_type& in_alloc,
            value_type&&          t) noexcept(noexcept(base::allocate_construct(std::move(t))))
            : base{in_alloc}
        {
            detail::var_base::storage.store(base::allocate_construct(std::move(t)), LSTM_RELAXED);
        }

        allocator_type get_allocator() const noexcept { return base::alloc(); }

        LSTM_REQUIRES(atomic)
        value_type unsafe_read() const noexcept
        {
            return base::load(detail::var_base::storage.load(LSTM_RELAXED));
        }

        LSTM_REQUIRES(!atomic)
        const value_type& unsafe_read() const noexcept
        {
            return base::load(detail::var_base::storage.load(LSTM_RELAXED));
        }

        void
        unsafe_write(const value_type& t) noexcept(noexcept(base::store(detail::var_base::storage,
                                                                        t)))
        {
            return base::store(detail::var_base::storage, t);
        }

        void unsafe_write(value_type&& t) noexcept(noexcept(base::store(detail::var_base::storage,
                                                                        std::move(t))))
        {
            return base::store(detail::var_base::storage, std::move(t));
        }
    };
LSTM_END

#endif /* LSTM_VAR_HPP */

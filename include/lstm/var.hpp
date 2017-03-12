#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>
#include <lstm/read_transaction.hpp>
#include <lstm/transaction.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var : private detail::var_alloc_policy<T, Alloc>
    {
    private:
        using base = detail::var_alloc_policy<T, Alloc>;

        friend struct ::lstm::detail::transaction_base;

    public:
        using value_type                 = T;
        using allocator_type             = Alloc;
        static constexpr bool     heap   = base::heap;
        static constexpr bool     atomic = base::atomic;
        static constexpr var_type type   = base::type;

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

        ///////////////////////////////////////////
        // DEFAULT CONSTRUCTORS
        ///////////////////////////////////////////
        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && std::is_default_constructible<allocator_type>{}
                      && detail::is_convertible<value_type>{})
        var() noexcept(noexcept(base::allocate_construct())
                       && std::is_nothrow_default_constructible<allocator_type>{})
        {
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && std::is_default_constructible<allocator_type>{}
                      && !detail::is_convertible<value_type>{})
        explicit var() noexcept(noexcept(base::allocate_construct())
                                && std::is_nothrow_default_constructible<allocator_type>{})
        {
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && detail::is_convertible<value_type>{})
        var(std::allocator_arg_t,
            const allocator_type& in_alloc) noexcept(noexcept(base::allocate_construct()))
            : base(std::allocator_arg, in_alloc)
        {
        }

        LSTM_REQUIRES(std::is_default_constructible<value_type>{}
                      && !detail::is_convertible<value_type>{})
        explicit var(std::allocator_arg_t,
                     const allocator_type& in_alloc) noexcept(noexcept(base::allocate_construct()))
            : base(std::allocator_arg, in_alloc)
        {
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
        var(std::initializer_list<Ilist> is,
            Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...))
                                 && std::is_nothrow_default_constructible<allocator_type>{})
            : base(is, (Us &&) us...)
        {
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && !detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && std::is_default_constructible<allocator_type>{}
                && sizeof...(Us) > 0)>
        explicit var(std::initializer_list<Ilist> is, Us&&... us) noexcept(
            noexcept(base::allocate_construct(is, (Us &&) us...))
            && std::is_nothrow_default_constructible<allocator_type>{})
            : base(is, (Us &&) us...)
        {
        }

        // support for var<std::vector<int>> = {1,2,3};
        template<
            typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<value_type, std::initializer_list<Ilist>&>{}
                           && detail::is_convertible<value_type, std::initializer_list<Ilist>&>{}
                           && std::is_default_constructible<allocator_type>{})>
        var(std::initializer_list<Ilist> is) noexcept(
            noexcept(base::allocate_construct(is))
            && std::is_nothrow_default_constructible<allocator_type>{})
            : base(is)
        {
        }

        template<
            typename Ilist,
            LSTM_REQUIRES_(std::is_constructible<value_type, std::initializer_list<Ilist>&>{}
                           && !detail::is_convertible<value_type, std::initializer_list<Ilist>&>{}
                           && std::is_default_constructible<allocator_type>{})>
        explicit var(std::initializer_list<Ilist> is) noexcept(
            noexcept(base::allocate_construct(is))
            && std::is_nothrow_default_constructible<allocator_type>{})
            : base(is)
        {
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{})>
        var(std::allocator_arg_t,
            const allocator_type&        in_alloc,
            std::initializer_list<Ilist> is,
            Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...)))
            : base(std::allocator_arg, in_alloc, is, (Us &&) us...)
        {
        }

        template<
            typename Ilist,
            typename... Us,
            LSTM_REQUIRES_(
                std::is_constructible<value_type, std::initializer_list<Ilist>&, Us&&...>{}
                && !detail::is_convertible<value_type, std::initializer_list<Ilist>&, Us&&...>{})>
        explicit var(std::allocator_arg_t,
                     const allocator_type&        in_alloc,
                     std::initializer_list<Ilist> is,
                     Us&&... us) noexcept(noexcept(base::allocate_construct(is, (Us &&) us...)))
            : base(std::allocator_arg, in_alloc, is, (Us &&) us...)
        {
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
        var(U&& u, Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u, (Us &&) us...))
                                        && std::is_nothrow_default_constructible<allocator_type>{})
            : base((U &&) u, (Us &&) us...)
        {
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && !detail::is_convertible<value_type, U&&, Us&&...>{}
                                && !std::is_same<detail::uncvref<U>, std::allocator_arg_t>{}
                                && std::is_default_constructible<allocator_type>{})>
        explicit var(U&& u, Us&&... us) noexcept(
            noexcept(base::allocate_construct((U &&) u, (Us &&) us...))
            && std::is_nothrow_default_constructible<allocator_type>{})
            : base((U &&) u, (Us &&) us...)
        {
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && detail::is_convertible<value_type, U&&, Us&&...>{})>
        var(std::allocator_arg_t,
            const allocator_type& in_alloc,
            U&&                   u,
            Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u, (Us &&) us...)))
            : base(std::allocator_arg, in_alloc, (U &&) u, (Us &&) us...)
        {
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(std::is_constructible<value_type, U&&, Us&&...>{}
                                && !detail::is_convertible<value_type, U&&, Us&&...>{})>
        explicit var(std::allocator_arg_t,
                     const allocator_type& in_alloc,
                     U&&                   u,
                     Us&&... us) noexcept(noexcept(base::allocate_construct((U &&) u,
                                                                            (Us &&) us...)))
            : base(std::allocator_arg, in_alloc, (U &&) u, (Us &&) us...)
        {
        }

        ///////////////////////////////////////////
        // BRACED INIT LIST CONSTRUCTORS
        ///////////////////////////////////////////
        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && detail::is_convertible<value_type, value_type&&>{}
                      && std::is_default_constructible<allocator_type>{})
        var(value_type&& t) noexcept(noexcept(base::allocate_construct(std::move(t)))
                                     && std::is_nothrow_default_constructible<allocator_type>{})
            : base(std::move(t))
        {
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && !detail::is_convertible<value_type, value_type&&>{}
                      && std::is_default_constructible<allocator_type>{})
        explicit var(value_type&& t) noexcept(
            noexcept(base::allocate_construct(std::move(t)))
            && std::is_nothrow_default_constructible<allocator_type>{})
            : base(std::move(t))
        {
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && detail::is_convertible<value_type, value_type&&>{})
        var(std::allocator_arg_t,
            const allocator_type& in_alloc,
            value_type&&          t) noexcept(noexcept(base::allocate_construct(std::move(t))))
            : base(std::allocator_arg, in_alloc, std::move(t))
        {
        }

        LSTM_REQUIRES(std::is_move_constructible<value_type>{}
                      && !detail::is_convertible<value_type, value_type&&>{})
        explicit var(std::allocator_arg_t,
                     const allocator_type& in_alloc,
                     value_type&& t) noexcept(noexcept(base::allocate_construct(std::move(t))))
            : base(std::allocator_arg, in_alloc, std::move(t))
        {
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

        LSTM_REQUIRES(atomic)
        LSTM_ALWAYS_INLINE value_type get(const transaction tx) const { return tx.read(*this); }

        LSTM_REQUIRES(!atomic)
        LSTM_ALWAYS_INLINE const value_type& get(const transaction tx) const
        {
            return tx.read(*this);
        }

        LSTM_REQUIRES(atomic)
        LSTM_ALWAYS_INLINE value_type get(const read_transaction tx) const
        {
            return tx.read(*this);
        }

        LSTM_REQUIRES(!atomic)
        LSTM_ALWAYS_INLINE const value_type& get(const read_transaction tx) const
        {
            return tx.read(*this);
        }

        LSTM_REQUIRES(atomic)
        LSTM_ALWAYS_INLINE value_type untracked_get(const transaction tx) const
        {
            return tx.untracked_read(*this);
        }

        LSTM_REQUIRES(!atomic)
        LSTM_ALWAYS_INLINE const value_type& untracked_get(const transaction tx) const
        {
            return tx.untracked_read(*this);
        }

        LSTM_REQUIRES(atomic)
        LSTM_ALWAYS_INLINE value_type untracked_get(const read_transaction tx) const
        {
            return tx.untracked_read(*this);
        }

        LSTM_REQUIRES(!atomic)
        LSTM_ALWAYS_INLINE const value_type& untracked_get(const read_transaction tx) const
        {
            return tx.untracked_read(*this);
        }

        template<typename U,
                 LSTM_REQUIRES_(std::is_assignable<value_type&, U&&>()
                                && std::is_constructible<value_type, U&&>())>
        LSTM_ALWAYS_INLINE void set(const transaction tx, U&& u)
        {
            tx.write(*this, (U &&) u);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename U,
                 LSTM_REQUIRES_(!std::is_assignable<value_type&, U&&>()
                                || !std::is_constructible<value_type, U&&>())>
        LSTM_ALWAYS_INLINE void set(const transaction, U&&)
        {
            static_assert(std::is_assignable<value_type&, U&&>(),
                          "set requires lstm::var<>::value_type be assignable by U");
            static_assert(std::is_constructible<value_type, U&&>(),
                          "set requires lstm::var<>::value_type be constructible by U");
        }
#endif /* LSTM_MAKE_SFINAE_FRIENDLY */
    };
LSTM_END

#endif /* LSTM_VAR_HPP */

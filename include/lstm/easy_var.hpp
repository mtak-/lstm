#ifndef LSTM_EASY_VAR_HPP
#define LSTM_EASY_VAR_HPP

#include <lstm/detail/easy_var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct easy_var : detail::easy_var_impl<T, Alloc>
    {
    private:
        using base = detail::easy_var_impl<T, Alloc>;

    public:
        using underlying_type            = typename base::underlying_type;
        using value_type                 = typename underlying_type::value_type;
        using allocator_type             = typename underlying_type::allocator_type;
        static constexpr bool     heap   = underlying_type::heap;
        static constexpr bool     atomic = underlying_type::atomic;
        static constexpr var_type type   = underlying_type::type;

        // TODO: write all these constructors out??? might make the interface more obvious
        using detail::easy_var_impl<T, Alloc>::easy_var_impl;

        easy_var(const easy_var& rhs)
            : base(rhs.get())
        {
        }

        easy_var& operator=(const easy_var& rhs)
        {
            lstm::read_write(
                [&](const transaction tx) { tx.write(underlying(), tx.read(rhs.underlying())); });
            return *this;
        }

        template<typename U = value_type,
                 LSTM_REQUIRES_(std::is_assignable<value_type&, const U&>())>
        easy_var& operator=(const U& rhs)
        {
            lstm::read_write([&](const transaction tx) { tx.write(underlying(), rhs); });
            return *this;
        }

        value_type get() const
        {
            return lstm::read_write([this](const transaction tx) { return tx.read(underlying()); });
        }

        inline operator value_type() const { return get(); }

        allocator_type get_allocator() const noexcept { return underlying().get_allocator(); }

        underlying_type&        underlying() & noexcept { return base::underlying(); }
        underlying_type&&       underlying() && noexcept { return std::move(base::underlying()); }
        const underlying_type&  underlying() const & noexcept { return base::underlying(); }
        const underlying_type&& underlying() const && noexcept
        {
            return std::move(base::underlying());
        }

        LSTM_REQUIRES(atomic)
        value_type unsafe_read() const noexcept { return underlying().unsafe_read(); }

        LSTM_REQUIRES(!atomic)
        const value_type& unsafe_read() const noexcept { return underlying().unsafe_read(); }

        void unsafe_write(const value_type& t) noexcept { return underlying().unsafe_write(t); }
        void unsafe_write(value_type&& t) noexcept
        {
            return underlying().unsafe_write(std::move(t));
        }
    };
LSTM_END

#endif /* LSTM_EASY_VAR_HPP */
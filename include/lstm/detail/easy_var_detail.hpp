#ifndef LSTM_DETAIL_EASY_VAR_DETAIL_HPP
#define LSTM_DETAIL_EASY_VAR_DETAIL_HPP

#include <lstm/lstm.hpp>

#define LSTM_ASSIGN_OP(symbol, concept)                                                            \
    template<typename U, LSTM_REQUIRES_(concept<U>{})>                                             \
    derived& operator symbol##=(const U u)                                                         \
    {                                                                                              \
        lstm::read_write(                                                                          \
            [&](const transaction tx) { underlying().set(tx, underlying().get(tx) symbol u); });   \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    template<typename U, typename UAlloc, LSTM_REQUIRES_(concept<U>{})>                            \
    derived& operator symbol##=(const easy_var<U, UAlloc>& uvar)                                   \
    {                                                                                              \
        lstm::read_write([&](const transaction tx) {                                               \
            underlying().set(tx, underlying().get(tx) symbol uvar.underlying().get(tx));           \
        });                                                                                        \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
/**/

#define LSTM_RMW_UNARY_OP(symbol)                                                                  \
    derived& operator symbol##symbol()                                                             \
    {                                                                                              \
        lstm::read_write([&](const transaction tx) {                                               \
            underlying().set(tx, underlying().get(tx) symbol T(1));                                \
        });                                                                                        \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    T operator symbol##symbol(int)                                                                 \
    {                                                                                              \
        return lstm::read_write([&](const transaction tx) {                                        \
            auto result = underlying().get(tx);                                                    \
            underlying().set(tx, result symbol T(1));                                              \
            return result;                                                                         \
        });                                                                                        \
    }                                                                                              \
/**/

LSTM_BEGIN
    template<typename T, typename Alloc = std::allocator<T>>
    struct easy_var;
LSTM_END

LSTM_DETAIL_BEGIN
    template<typename T,
             typename Alloc,
             bool = std::is_arithmetic<T>{},
             bool = std::is_integral<T>{}>
    struct easy_var_impl : private var<T, Alloc>
    {
    protected:
        using underlying_type = var<T, Alloc>;
        using derived         = easy_var<T, Alloc>;

        using var<T, Alloc>::var;

        underlying_type&        underlying() & noexcept { return *this; }
        underlying_type&&       underlying() && noexcept { return std::move(*this); }
        const underlying_type&  underlying() const & noexcept { return *this; }
        const underlying_type&& underlying() const && noexcept { return std::move(*this); }
    };

    template<typename T, typename Alloc>
    struct easy_var_impl<T, Alloc, true, false> : easy_var_impl<T, Alloc, false, false>
    {
    protected:
        using underlying_type = typename easy_var_impl<T, Alloc, false, false>::underlying_type;
        using derived         = typename easy_var_impl<T, Alloc, false, false>::derived;
        using easy_var_impl<T, Alloc, false, false>::underlying;
        using easy_var_impl<T, Alloc, false, false>::easy_var_impl;

    public:
        LSTM_ASSIGN_OP(+, std::is_arithmetic)
        LSTM_ASSIGN_OP(-, std::is_arithmetic)
        LSTM_ASSIGN_OP(*, std::is_arithmetic)
        LSTM_ASSIGN_OP(/, std::is_arithmetic)

        LSTM_RMW_UNARY_OP(+) // ++
        LSTM_RMW_UNARY_OP(-) // --
    };

    template<typename T, typename Alloc>
    struct easy_var_impl<T, Alloc, true, true> : easy_var_impl<T, Alloc, true, false>
    {
    protected:
        using underlying_type = typename easy_var_impl<T, Alloc, true, false>::underlying_type;
        using derived         = typename easy_var_impl<T, Alloc, true, false>::derived;
        using easy_var_impl<T, Alloc, true, false>::underlying;
        using easy_var_impl<T, Alloc, true, false>::easy_var_impl;

    public:
        // clang-format off
        LSTM_ASSIGN_OP(%,  std::is_integral)
        LSTM_ASSIGN_OP(&,  std::is_integral)
        LSTM_ASSIGN_OP(|,  std::is_integral)
        LSTM_ASSIGN_OP(^,  std::is_integral)
        LSTM_ASSIGN_OP(>>, std::is_integral)
        LSTM_ASSIGN_OP(<<, std::is_integral)
        // clang-format on
    };
LSTM_DETAIL_END

#undef LSTM_ASSIGN_OP
#undef LSTM_UNARY_OP
#undef LSTM_RMW_UNARY_OP
#undef LSTM_BINARY_ARITH_OP

#endif
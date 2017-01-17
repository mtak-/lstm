#ifndef LSTM_DETAIL_EASY_VAR_DETAIL_HPP
#define LSTM_DETAIL_EASY_VAR_DETAIL_HPP

#include <lstm/lstm.hpp>

#define LSTM_ASSIGN_OP(symbol, concept)                                                            \
    template<typename U,                                                                           \
        LSTM_REQUIRES_(concept <U>{})>                                                             \
    derived& operator symbol##= (const U u) {                                                      \
        lstm::read_write([&](auto& tx) {                                                           \
            tx.write(underlying(), tx.read(underlying()) symbol u);                                \
        });                                                                                        \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    template<typename U, typename UAlloc,                                                          \
        LSTM_REQUIRES_(concept <U>{})>                                                             \
    derived& operator symbol##= (const easy_var<U, UAlloc>& uvar) {                                \
        lstm::read_write([&](auto& tx) {                                                           \
            tx.write(underlying(), tx.read(underlying()) symbol tx.read(uvar.underlying()));       \
        });                                                                                        \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    /**/
    
#define LSTM_RMW_UNARY_OP(symbol)                                                                  \
    derived& operator symbol##symbol () {                                                          \
        lstm::read_write([&](auto& tx) {                                                           \
            tx.write(underlying(), tx.read(underlying()) symbol T(1));                             \
        });                                                                                        \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    T operator symbol##symbol (int) {                                                              \
        return lstm::read_write([&](auto& tx) {                                                    \
            auto result = tx.read(underlying());                                                   \
            tx.write(underlying(), result symbol T(1));                                            \
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
    struct easy_var_impl : private var<T, Alloc> {
    protected:
        using var_t = var<T, Alloc>;
        using derived = easy_var<T, Alloc>;
        
    public:
        using var_t::var;
        
        lstm::var<T, Alloc>& underlying() & noexcept { return *this; }
        lstm::var<T, Alloc>&& underlying() && noexcept { return std::move(*this); }
        const lstm::var<T, Alloc>& underlying() const & noexcept { return *this; }
        const lstm::var<T, Alloc>&& underlying() const && noexcept { return std::move(*this); }
    };
    
    template<typename T, typename Alloc>
    struct easy_var_impl<T, Alloc, true, false> : easy_var_impl<T, Alloc, false, false> {
    protected:
        using derived = easy_var<T, Alloc>;
        
    public:
        using easy_var_impl<T, Alloc, false, false>::easy_var_impl;
        using easy_var_impl<T, Alloc, false, false>::underlying;
        
        LSTM_ASSIGN_OP(+, std::is_arithmetic)
        LSTM_ASSIGN_OP(-, std::is_arithmetic)
        LSTM_ASSIGN_OP(*, std::is_arithmetic)
        LSTM_ASSIGN_OP(/, std::is_arithmetic)
        
        LSTM_RMW_UNARY_OP(+) // ++
        LSTM_RMW_UNARY_OP(-) // --
    };
    
    template<typename T, typename Alloc>
    struct easy_var_impl<T, Alloc, true, true> : easy_var_impl<T, Alloc, true, false> {
    protected:
        using derived = easy_var<T, Alloc>;
        
    public:
        using easy_var_impl<T, Alloc, true, false>::easy_var_impl;
        using easy_var_impl<T, Alloc, true, false>::underlying;
        
        LSTM_ASSIGN_OP(%, std::is_integral)
        LSTM_ASSIGN_OP(&, std::is_integral)
        LSTM_ASSIGN_OP(|, std::is_integral)
        LSTM_ASSIGN_OP(^, std::is_integral)
        LSTM_ASSIGN_OP(>>, std::is_integral)
        LSTM_ASSIGN_OP(<<, std::is_integral)
    };
LSTM_DETAIL_END

#undef LSTM_ASSIGN_OP
#undef LSTM_UNARY_OP
#undef LSTM_RMW_UNARY_OP
#undef LSTM_BINARY_ARITH_OP

#endif
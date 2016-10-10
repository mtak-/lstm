#ifndef LSTM_DETAIL_EASY_VAR_DETAIL_HPP
#define LSTM_DETAIL_EASY_VAR_DETAIL_HPP

#include <lstm/lstm.hpp>

#define LSTM_ASSIGN_OP(symbol, concept)                                                            \
    template<typename U,                                                                           \
        LSTM_REQUIRES_(concept <U>{})>                                                             \
    derived& operator symbol##= (const U u) {                                                      \
        lstm::atomic([&](auto& tx) { tx.store(var, tx.load(var) symbol u); });                     \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    template<typename U, typename UAlloc,                                                          \
        LSTM_REQUIRES_(concept <U>{})>                                                             \
    derived& operator symbol##= (const easy_var<U, UAlloc>& uvar) {                                \
        lstm::atomic([&](auto& tx) { tx.store(var, tx.load(var) symbol tx.load(uvar)); });         \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    /**/
    
#define LSTM_RMW_UNARY_OP(symbol)                                                                  \
    derived& operator symbol##symbol () {                                                          \
        lstm::atomic([&](auto& tx) { tx.store(var, tx.load(var) symbol T(1)); });                  \
        return static_cast<derived&>(*this);                                                       \
    }                                                                                              \
    T operator symbol##symbol (int) {                                                              \
        return lstm::atomic([&](auto& tx) {                                                        \
            auto result = tx.load(var);                                                            \
            tx.store(var, result symbol T(1));                                                     \
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
    struct easy_var_impl {
    protected:
        using var_t = var<T, Alloc>;
        var_t var;
        using derived = easy_var<T, Alloc>;
        
    public:
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<var_t, Us&&...>())>
        easy_var_impl(Us&&... us) noexcept(std::is_nothrow_constructible<var_t, Us&&...>{})
            : var((Us&&)us...)
        {}
        
        lstm::var<T, Alloc>& underlying() & noexcept { return var; }
        lstm::var<T, Alloc>&& underlying() && noexcept { return std::move(var); }
        const lstm::var<T, Alloc>& underlying() const & noexcept { return var; }
        const lstm::var<T, Alloc>&& underlying() const && noexcept { return std::move(var); }
    };
    
    template<typename T, typename Alloc>
    struct easy_var_impl<T, Alloc, true, false> : easy_var_impl<T, Alloc, false, false> {
    protected:
        using easy_var_impl<T, Alloc, false, false>::var;
        using derived = easy_var<T, Alloc>;
        
    public:
        using easy_var_impl<T, Alloc, false, false>::easy_var_impl;
        
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
        using easy_var_impl<T, Alloc, true, false>::var;
        using derived = easy_var<T, Alloc>;
        
    public:
        using easy_var_impl<T, Alloc, true, false>::easy_var_impl;
        
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
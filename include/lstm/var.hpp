#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var : detail::var_impl<T, Alloc> {
        static_assert(!std::is_rvalue_reference<T>{},
            "cannot construct a var from an rvalue reference");
        static_assert(std::is_same<std::remove_reference_t<T>, typename Alloc::value_type>{},
            "invalid allocator for type T");
        static_assert(!std::is_array<T>{},
            "raw c arrays are not allowed. try using a c array reference, or a std::array");
        
        using detail::var_impl<T, Alloc>::var_impl;
    };
LSTM_END

#endif /* LSTM_VAR_HPP */
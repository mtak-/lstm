#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T>
    struct var : detail::var_impl<T> {
        using detail::var_impl<T>::var_impl;
        using detail::var_impl<T>::operator=;
    };
LSTM_END

#endif /* LSTM_VAR_HPP */
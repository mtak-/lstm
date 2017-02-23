#ifndef LSTM_RETRY_HPP
#define LSTM_RETRY_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_BEGIN
    [[noreturn]] LSTM_NOINLINE void retry()
    {
        LSTM_USER_FAIL_TX();
        throw detail::tx_retry{};
    }
LSTM_END

#endif /* LSTM_RETRY_HPP */
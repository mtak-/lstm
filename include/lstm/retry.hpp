#ifndef LSTM_RETRY_HPP
#define LSTM_RETRY_HPP

#include <lstm/detail/backoff.hpp>

LSTM_BEGIN
    [[noreturn]] LSTM_NOINLINE_LUKEWARM inline void retry()
    {
        LSTM_PERF_STATS_USER_FAILURES();
        throw detail::tx_retry{};
    }
LSTM_END

#endif /* LSTM_RETRY_HPP */
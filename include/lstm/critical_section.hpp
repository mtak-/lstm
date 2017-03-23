#ifndef LSTM_CRITICAL_SECTION_HPP
#define LSTM_CRITICAL_SECTION_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    struct critical_section
    {
        bool valid(const thread_data& td) const noexcept
        {
            return td.in_critical_section() && !td.in_transaction();
        }
    };
LSTM_END

#endif /* LSTM_CRITICAL_SECTION_HPP */

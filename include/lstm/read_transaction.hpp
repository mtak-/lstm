#ifndef LSTM_READ_TRANSACTION_HPP
#define LSTM_READ_TRANSACTION_HPP

#include <lstm/detail/transaction_base.hpp>

LSTM_BEGIN
    struct read_transaction : private detail::transaction_base
    {
        template<typename, typename>
        friend struct ::lstm::var;

        inline read_transaction(thread_data& in_tls_td, const epoch_t in_version) noexcept
            : transaction_base(&in_tls_td, in_version)
        {
        }

        explicit inline read_transaction(const epoch_t in_version) noexcept
            : transaction_base(nullptr, in_version)
        {
        }

        bool    nested_in_rw() const noexcept { return can_write(); }
        epoch_t version() const noexcept { return transaction_base::version(); }

        bool valid(const thread_data& td) const noexcept { return transaction_base::valid(&td); }
        bool read_valid(const epoch_t version) const noexcept { return rw_valid(version); }
        bool read_valid(const detail::var_base& v) const noexcept { return rw_valid(v); }
    };
LSTM_END

#endif /* LSTM_READ_TRANSACTION_HPP */

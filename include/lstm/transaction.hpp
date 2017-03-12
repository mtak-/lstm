#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/read_transaction.hpp>

LSTM_BEGIN
    struct transaction : private detail::transaction_base
    {
        template<typename, typename>
        friend struct ::lstm::var;

        inline transaction(thread_data& in_tls_td, const gp_t in_version) noexcept
            : transaction_base(&in_tls_td, in_version)
        {
        }

        explicit operator read_transaction() const noexcept
        {
            return {get_thread_data(), version()};
        }

        read_transaction unsafe_unchecked_demote() const noexcept
        {
            LSTM_ASSERT(can_demote_safely());
            return read_transaction{version()};
        }

        read_transaction unsafe_checked_demote() const noexcept
        {
            if (can_demote_safely())
                return read_transaction{version()};
            return read_transaction{get_thread_data(), version()};
        }

        thread_data& get_thread_data() const noexcept
        {
            return transaction_base::get_thread_data();
        }

        gp_t version() const noexcept { return transaction_base::version(); }

        void reset_version(const gp_t new_version) noexcept
        {
            transaction_base::reset_version(new_version);
        }

        bool valid(const thread_data& td) const noexcept { return transaction_base::valid(&td); }
        bool read_write_valid(const gp_t version) const noexcept { return rw_valid(version); }
        bool read_write_valid(const detail::var_base& v) const noexcept { return rw_valid(v); }
        bool read_valid(const gp_t version) const noexcept { return rw_valid(version); }
        bool read_valid(const detail::var_base& v) const noexcept { return rw_valid(v); }
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

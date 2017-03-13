#ifndef LSTM_READ_TRANSACTION_HPP
#define LSTM_READ_TRANSACTION_HPP

#include <lstm/detail/transaction_base.hpp>

LSTM_BEGIN
    struct read_transaction : private detail::transaction_base
    {
        template<typename, typename>
        friend struct ::lstm::var;

        inline read_transaction(thread_data& in_tls_td, const gp_t in_version) noexcept
            : transaction_base(&in_tls_td, in_version)
        {
        }

        explicit inline read_transaction(const gp_t in_version) noexcept
            : transaction_base(nullptr, in_version)
        {
        }

        bool nested_in_rw() const noexcept { return can_write(); }
        gp_t version() const noexcept { return transaction_base::version(); }
        void reset_version(const gp_t new_version) noexcept
        {
            transaction_base::reset_version(new_version);
        }

        bool valid(const thread_data& td) const noexcept { return transaction_base::valid(&td); }
        bool read_valid(const gp_t version) const noexcept { return rw_valid(version); }
        bool read_valid(const detail::var_base& v) const noexcept { return rw_valid(v); }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void sometime_after(Func&& func) const
            noexcept(noexcept(transaction_base::sometime_after((Func &&) func)))
        {
            transaction_base::sometime_after((Func &&) func);
        }

        template<typename Func,
                 LSTM_REQUIRES_(std::is_constructible<detail::gp_callback, Func&&>{})>
        void after_fail(Func&& func) const
            noexcept(noexcept(transaction_base::after_fail((Func &&) func)))
        {
            transaction_base::after_fail((Func &&) func);
        }
    };
LSTM_END

#endif /* LSTM_READ_TRANSACTION_HPP */

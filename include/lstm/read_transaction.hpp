#ifndef LSTM_READ_TRANSACTION_HPP
#define LSTM_READ_TRANSACTION_HPP

#include <lstm/detail/transaction_base.hpp>

LSTM_BEGIN
    struct read_transaction : private detail::transaction_base
    {
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

        bool valid(const thread_data& td) const noexcept { return transaction_base::valid(td); }
        bool read_valid(const gp_t version) const noexcept { return rw_valid(version); }
        bool read_valid(const detail::var_base& v) const noexcept { return rw_valid(v); }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_NOINLINE const T& read(const var<T, Alloc>& src_var) const
        {
            return ro_read(src_var);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc>& src_var) const
        {
            return ro_read(src_var);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& untracked_read(const var<T, Alloc>& src_var) const
        {
            return ro_untracked_read(src_var);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T untracked_read(const var<T, Alloc>& src_var) const
        {
            return ro_untracked_read(src_var);
        }

        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc>
        void read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void read(const var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void untracked_read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void untracked_read(const var<T, Alloc>&& v) const = delete;
    };
LSTM_END

#endif /* LSTM_READ_TRANSACTION_HPP */

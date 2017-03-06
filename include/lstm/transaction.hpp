#ifndef LSTM_TRANSACTION_HPP
#define LSTM_TRANSACTION_HPP

#include <lstm/read_transaction.hpp>

LSTM_BEGIN
    struct transaction : private detail::transaction_base
    {
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

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& read(const var<T, Alloc>& src_var) const
        {
            return rw_read(src_var);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc>& src_var) const
        {
            return rw_read(src_var);
        }

        template<typename T,
                 typename Alloc,
                 typename U = T,
                 LSTM_REQUIRES_(std::is_assignable<T&, U&&>() && std::is_constructible<T, U&&>())>
        LSTM_ALWAYS_INLINE void write(var<T, Alloc>& dest_var, U&& u) const
        {
            rw_write(dest_var, (U &&) u);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& untracked_read(const var<T, Alloc>& src_var) const
        {
            return rw_untracked_read(src_var);
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T untracked_read(const var<T, Alloc>& src_var) const
        {
            return rw_untracked_read(src_var);
        }

        // reading/writing an rvalue probably never makes sense
        template<typename T, typename Alloc>
        void read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void read(const var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void write(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void write(const var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void untracked_read(var<T, Alloc>&& v) const = delete;
        template<typename T, typename Alloc>
        void untracked_read(const var<T, Alloc>&& v) const = delete;
    };
LSTM_END

#endif /* LSTM_TRANSACTION_HPP */

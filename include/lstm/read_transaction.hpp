#ifndef LSTM_READ_TRANSACTION_HPP
#define LSTM_READ_TRANSACTION_HPP

#include <lstm/transaction.hpp>

LSTM_BEGIN
    struct read_transaction : private transaction
    {
    private:
        LSTM_NOINLINE detail::var_storage read_impl_slow_path(const detail::var_base& src_var) const
        {
            if (nested_in_rw())
                return transaction::read_impl(src_var);
            else
                detail::internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM detail::var_storage read_impl(const detail::var_base& src_var) const
        {
            assert(valid());

            if (LSTM_UNLIKELY(nested_in_rw()))
                return read_impl_slow_path(src_var);

            const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
            if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                return result;
            return read_impl_slow_path(src_var);
        }

        LSTM_NOINLINE detail::var_storage
        untracked_read_impl_slow_path(const detail::var_base& src_var) const
        {
            if (nested_in_rw())
                return transaction::untracked_read_impl(src_var);
            else
                detail::internal_retry();
        }

        LSTM_NOINLINE_LUKEWARM detail::var_storage
        untracked_read_impl(const detail::var_base& src_var) const
        {
            assert(valid());

            if (LSTM_UNLIKELY(nested_in_rw()))
                return untracked_read_impl_slow_path(src_var);

            const detail::var_storage result = src_var.storage.load(LSTM_ACQUIRE);
            if (LSTM_LIKELY(rw_valid(src_var.version_lock.load(LSTM_ACQUIRE))))
                return result;
            return untracked_read_impl_slow_path(src_var);
        }

    public:
        inline read_transaction(const gp_t in_version) noexcept
            : transaction(transaction::read_only, in_version)
        {
        }

        bool nested_in_rw() const noexcept { return transaction::raw_tls_td_ptr(); }
        gp_t version() const noexcept { return transaction::version(); }
        void reset_version(const gp_t new_version) noexcept
        {
            transaction::reset_version(new_version);
        }

        bool valid(const thread_data& td = tls_thread_data()) const noexcept
        {
            if (nested_in_rw())
                return transaction::valid(td);
            return td.gp() == version() && td.in_transaction();
        }

        bool rw_valid(const gp_t version) const noexcept { return transaction::rw_valid(version); }
        bool rw_valid(const detail::var_base& v) const noexcept { return transaction::rw_valid(v); }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_NOINLINE const T& read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(!var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE const T& untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(untracked_read_impl(src_var));
        }

        template<typename T, typename Alloc, LSTM_REQUIRES_(var<T, Alloc>::atomic)>
        LSTM_ALWAYS_INLINE T untracked_read(const var<T, Alloc>& src_var) const
        {
            static_assert(!std::is_reference<decltype(
                              var<T, Alloc>::load(src_var.storage.load()))>{},
                          "");
            return var<T, Alloc>::load(untracked_read_impl(src_var));
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

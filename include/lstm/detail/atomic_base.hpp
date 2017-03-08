#ifndef LSTM_DETAIL_ATOMIC_BASE_HPP
#define LSTM_DETAIL_ATOMIC_BASE_HPP

#include <lstm/thread_data.hpp>
#include <lstm/transaction_domain.hpp>

LSTM_DETAIL_BEGIN
    struct atomic_base_fn
    {
        template<typename Func,
                 typename Tx,
                 typename... Args,
                 LSTM_REQUIRES_(callable_with_tx<Func&&, Tx, Args&&...>())>
        static transact_result<Func, Tx, Args&&...> call(Func&& func, const Tx tx, Args&&... args)
        {
            return ((Func &&) func)(tx, (Args &&) args...);
        }

        template<typename Func,
                 typename Tx,
                 typename... Args,
                 LSTM_REQUIRES_(!callable_with_tx<Func&&, Tx, Args&&...>())>
        static transact_result<Func, Tx, Args&&...> call(Func&& func, const Tx, Args&&... args)
        {
            return ((Func &&) func)((Args &&) args...);
        }

        template<tx_kind kind>
        static gp_t tx_start(thread_data& tls_td, transaction_domain& domain) noexcept
        {
            static_assert(kind != tx_kind::none);

            const gp_t version = domain.get_clock();
            tls_td.access_lock(version);
            tls_td.tx_state = kind;
            return version;
        }

        template<tx_kind kind>
        static void tx_failure(thread_data& tls_td) noexcept
        {
            static_assert(kind != tx_kind::none);

            if (kind != tx_kind::read_only)
                tls_td.clear_read_write_sets();
            tls_td.succ_callbacks.active().callbacks.clear();
            tls_td.do_fail_callbacks();
        }

        template<tx_kind         kind>
        [[noreturn]] static void unhandled_exception(thread_data& tls_td)
        {
            tls_td.access_unlock();
            tls_td.tx_state = tx_kind::none;
            tx_failure<kind>(tls_td);
            throw;
        }

        template<tx_kind kind>
        LSTM_ALWAYS_INLINE static gp_t
        tx_restart(thread_data& tls_td, transaction_domain& domain) noexcept
        {
            static_assert(kind != tx_kind::none);

            tx_failure<kind>(tls_td);
            const gp_t new_version = domain.get_clock();
            tls_td.access_relock(new_version);
            return new_version;
        }

        template<tx_kind kind>
        static void tx_success(thread_data& tls_td, const gp_t sync_version) noexcept
        {
            static_assert(kind != tx_kind::none);

            tls_td.access_unlock();
            tls_td.tx_state = tx_kind::none;

            if (kind != tx_kind::read_only)
                tls_td.clear_read_write_sets();
            tls_td.fail_callbacks.clear();
            tls_td.reclaim(sync_version);
        }

        static bool valid_start_state(thread_data& tls_td) noexcept
        {
            return tls_td.read_set.empty() && tls_td.write_set.empty()
                   && tls_td.fail_callbacks.empty()
                   && tls_td.succ_callbacks.active().callbacks.empty();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_ATOMIC_BASE_HPP */

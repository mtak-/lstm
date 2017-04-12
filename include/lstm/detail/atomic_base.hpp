#ifndef LSTM_DETAIL_ATOMIC_BASE_HPP
#define LSTM_DETAIL_ATOMIC_BASE_HPP

#include <lstm/detail/transaction_domain.hpp>

#include <lstm/thread_data.hpp>

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

        static void set_rw(thread_data& tls_td) noexcept { tls_td.tx_state = tx_kind::read_write; }
        static void set_read(thread_data& tls_td) noexcept { tls_td.tx_state = tx_kind::read_only; }

        template<tx_kind kind>
        static void tx_failure_no_backoff(thread_data& tls_td) noexcept
        {
            static_assert(kind != tx_kind::none);

            tls_td.access_unlock();

            LSTM_PERF_STATS_FAILURES();
            LSTM_PERF_STATS_READS(tls_td.read_set.size());
            LSTM_PERF_STATS_MAX_READ_SIZE(tls_td.read_set.size());
            LSTM_PERF_STATS_WRITES(tls_td.write_set.size());
            LSTM_PERF_STATS_MAX_WRITE_SIZE(tls_td.write_set.size());

            if (kind != tx_kind::read_only) {
                tls_td.clear_read_write_sets();
                tls_td.succ_callbacks.clear_working_epoch();
                tls_td.do_fail_callbacks();
            } else {
                LSTM_ASSERT(tls_td.succ_callbacks.working_epoch_empty());
                LSTM_ASSERT(tls_td.fail_callbacks.empty());
                LSTM_ASSERT(tls_td.read_set.empty());
                LSTM_ASSERT(tls_td.write_set.empty());
            }
        }

        template<tx_kind kind>
        static void tx_failure(thread_data& tls_td) noexcept
        {
            tx_failure_no_backoff<kind>(tls_td);
            default_backoff{}();
        }

        template<tx_kind         kind>
        [[noreturn]] static void unhandled_exception(thread_data& tls_td)
        {
            tx_failure_no_backoff<kind>(tls_td);

            tls_td.tx_state = tx_kind::none;

            throw;
        }

        template<tx_kind kind>
        static void tx_success(thread_data& tls_td, const epoch_t sync_epoch) noexcept
        {
            static_assert(kind != tx_kind::none);

            tls_td.access_unlock();
            tls_td.tx_state = tx_kind::none;

            LSTM_PERF_STATS_SUCCESSES();
            LSTM_PERF_STATS_READS(tls_td.read_set.size());
            LSTM_PERF_STATS_MAX_READ_SIZE(tls_td.read_set.size());
            LSTM_PERF_STATS_WRITES(tls_td.write_set.size());
            LSTM_PERF_STATS_MAX_WRITE_SIZE(tls_td.write_set.size());

            if (kind != tx_kind::read_only) {
                tls_td.clear_read_write_sets();
                tls_td.fail_callbacks.clear();
                tls_td.reclaim(sync_epoch);
            } else {
                LSTM_ASSERT(tls_td.succ_callbacks.working_epoch_empty());
                LSTM_ASSERT(tls_td.fail_callbacks.empty());
                LSTM_ASSERT(tls_td.read_set.empty());
                LSTM_ASSERT(tls_td.write_set.empty());
            }
        }

        static bool valid_start_state(thread_data& tls_td) noexcept
        {
            return tls_td.read_set.empty() && tls_td.write_set.empty()
                   && tls_td.fail_callbacks.empty() && tls_td.succ_callbacks.working_epoch_empty();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_ATOMIC_BASE_HPP */

#ifndef LSTM_DETAIL_COMMIT_ALGORITHM_HPP
#define LSTM_DETAIL_COMMIT_ALGORITHM_HPP

#include <lstm/thread_data.hpp>
#include <lstm/transaction.hpp>
#include <lstm/transaction_domain.hpp>

LSTM_DETAIL_BEGIN
    struct commit_algorithm
    {
    private:
        using write_set_iter = typename thread_data::write_set_iter;

        static inline bool lock(var_base& v, const transaction tx) noexcept
        {
            gp_t version_buf = v.version_lock.load(LSTM_RELAXED);
            return tx.rw_valid(version_buf)
                   && v.version_lock.compare_exchange_strong(version_buf,
                                                             as_locked(version_buf),
                                                             LSTM_ACQUIRE,
                                                             LSTM_RELAXED);
        }

        // x86_64: likely compiles to mov
        static inline void unlock(var_base& v, const gp_t version_to_set) noexcept
        {
            assert(locked(v.version_lock.load(LSTM_RELAXED)));
            assert(!locked(version_to_set));

            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }

        // x86: likely compiles to xor
        static inline void unlock(var_base& v) noexcept
        {
            const gp_t locked_version = v.version_lock.load(LSTM_RELAXED);

            assert(locked(locked_version));

            v.version_lock.store(locked_version ^ lock_bit, LSTM_RELEASE);
        }

        static void unlock_write_set(write_set_iter begin, const write_set_iter end) noexcept
        {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }

        static bool commit_lock_writes(const transaction tx) noexcept
        {
            thread_data&         tls_td      = tx.get_thread_data();
            write_set_iter       write_begin = tls_td.write_set.begin();
            const write_set_iter write_end   = tls_td.write_set.end();

            for (write_set_iter write_iter = write_begin; write_iter != write_end; ++write_iter) {
                if (!lock(write_iter->dest_var(), tx)) {
                    unlock_write_set(write_begin, write_iter);
                    return false;
                }
            }

            return true;
        }

        static bool commit_validate_reads(const transaction tx) noexcept
        {
            thread_data& tls_td = tx.get_thread_data();
            for (read_set_value_type read_set_vaue : tls_td.read_set) {
                if (!tx.rw_valid(read_set_vaue.src_var())) {
                    unlock_write_set(tls_td.write_set.begin(), tls_td.write_set.end());
                    return false;
                }
            }
            return true;
        }

        static void commit_publish(thread_data& tls_td, const gp_t write_version) noexcept
        {
            for (write_set_value_type write_set_value : tls_td.write_set) {
                write_set_value.dest_var().storage.store(write_set_value.pending_write(),
                                                         LSTM_RELAXED);
                unlock(write_set_value.dest_var(), write_version);
            }
        }

        static void commit_reclaim_slow_path(thread_data& tls_td, const gp_t sync_version) noexcept
        {
            tls_td.synchronize(sync_version);
            tls_td.do_succ_callbacks();
        }

        static void commit_reclaim(thread_data& tls_td, const gp_t sync_version) noexcept
        {
            // TODO: batching
            if (!tls_td.succ_callbacks.empty())
                commit_reclaim_slow_path(tls_td, sync_version);
        }

        static void
        commit_slowerer_path(thread_data& tls_td, const gp_t prev_write_version) noexcept
        {
            commit_publish(tls_td, prev_write_version + 1);

            tls_td.access_unlock();

            tls_td.write_set.clear();

            commit_reclaim(tls_td, prev_write_version);
        }

        static bool commit_slower_path(const transaction tx, transaction_domain& domain) noexcept
        {
            const gp_t prev_write_version = domain.fetch_and_bump_clock();

            if (prev_write_version != tx.version() && !commit_validate_reads(tx))
                return false;

            commit_slowerer_path(tx.get_thread_data(), prev_write_version);

            return true;
        }

        static bool commit_slow_path(const transaction tx, transaction_domain& domain) noexcept
        {
            if (!commit_lock_writes(tx))
                return false;
            return commit_slower_path(tx, domain);
        }

    public:
        static bool try_commit(const transaction tx, transaction_domain& domain) noexcept
        {
            thread_data& tls_td = tx.get_thread_data();
            if (!tls_td.write_set.empty() && !commit_slow_path(tx, domain)) {
                LSTM_INTERNAL_FAIL_TX();
                return false;
            }

            assert(tls_td.succ_callbacks.empty()); // TODO: probly should handle not empty case
            tls_td.read_set.clear();
            tls_td.fail_callbacks.clear();
            tls_td.access_unlock();

            LSTM_SUCC_TX();

            return true;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_COMMIT_ALGORITHM_HPP */
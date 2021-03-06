#ifndef LSTM_DETAIL_COMMIT_ALGORITHM_HPP
#define LSTM_DETAIL_COMMIT_ALGORITHM_HPP

#include <lstm/detail/transaction_domain.hpp>

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    namespace
    {
        static constexpr epoch_t commit_failed = off_state;
    }

    struct commit_algorithm
    {
    private:
        using write_set_t         = typename thread_data::write_set_t;
        using write_set_iter      = typename thread_data::write_set_iter;
        using read_set_const_iter = typename thread_data::read_set_const_iter;

        commit_algorithm()                        = delete;
        commit_algorithm(const commit_algorithm&) = delete;
        commit_algorithm& operator=(const commit_algorithm&) = delete;
        ~commit_algorithm()                                  = delete;

        static inline bool lock(var_base& v, const transaction tx) noexcept
        {
            epoch_t version_buf = v.version_lock.load(LSTM_RELAXED);
            return tx.read_write_valid(version_buf)
                   && v.version_lock.compare_exchange_strong(version_buf,
                                                             as_locked(version_buf),
                                                             LSTM_ACQUIRE,
                                                             LSTM_RELAXED);
        }

        // x86_64: likely compiles to mov
        static inline void unlock_as_version(var_base& v, const epoch_t version_to_set) noexcept
        {
            LSTM_ASSERT(locked(v.version_lock.load(LSTM_RELAXED)));
            LSTM_ASSERT(!locked(version_to_set));

            v.version_lock.store(version_to_set, LSTM_RELEASE);
        }

        // x86: likely compiles to xor
        static inline void unlock(var_base& v) noexcept
        {
            unlock_as_version(v, v.version_lock.load(LSTM_RELAXED) ^ lock_bit);
        }

        LSTM_NOINLINE static void
        unlock_write_set(write_set_iter begin, const write_set_iter end) noexcept
        {
            for (; begin != end; ++begin)
                unlock(begin->dest_var());
        }

        static void remove_writes_from_reads(thread_data& tls_td) noexcept
        {
            const read_set_const_iter begin = tls_td.read_set.begin();
            for (read_set_const_iter read_iter = tls_td.read_set.end(); read_iter != begin;) {
                --read_iter;
                if (tls_td.write_set.end() != tls_td.write_set.find(read_iter->src_var()))
                    tls_td.read_set.unordered_erase(read_iter);
            }
        }

        static bool lock_writes(const transaction tx) noexcept
        {
            thread_data&         tls_td      = tx.get_thread_data();
            write_set_iter       write_begin = tls_td.write_set.begin();
            const write_set_iter write_end   = tls_td.write_set.end();

            for (write_set_iter write_iter = write_begin; write_iter != write_end; ++write_iter) {
                if (LSTM_UNLIKELY(!lock(write_iter->dest_var(), tx))) {
                    unlock_write_set(write_begin, write_iter);
                    return false;
                }
            }

            return true;
        }

        static bool validate_reads(const transaction tx) noexcept
        {
            thread_data& tls_td = tx.get_thread_data();
            for (const read_set_value_type read_set_value : tls_td.read_set) {
                if (LSTM_UNLIKELY(!tx.read_write_valid(read_set_value.src_var()))) {
                    unlock_write_set(tls_td.write_set.begin(), tls_td.write_set.end());
                    return false;
                }
            }
            return true;
        }

        static void do_writes(const write_set_t& write_set) noexcept
        {
            for (write_set_value_type write_set_value : write_set)
                write_set_value.dest_var().storage.store(write_set_value.pending_write(),
                                                         LSTM_RELEASE);
        }

        static void publish(const write_set_t& write_set, const epoch_t write_version) noexcept
        {
            for (write_set_value_type write_set_value : write_set)
                unlock_as_version(write_set_value.dest_var(), write_version);
        }

        static epoch_t slower_path(const transaction tx) noexcept
        {
            // last check
            if (!validate_reads(tx))
                return commit_failed;

            const write_set_t& write_set = tx.get_thread_data().write_set;

            do_writes(write_set);

            const epoch_t sync_epoch = default_domain().fetch_and_bump_clock();
            LSTM_ASSERT(tx.version() <= sync_epoch);

            publish(write_set, sync_epoch + transaction_domain::bump_size());

            return sync_epoch;
        }

        static epoch_t slow_path(const transaction tx) noexcept
        {
            remove_writes_from_reads(tx.get_thread_data());
            if (!lock_writes(tx))
                return commit_failed;
            return slower_path(tx);
        }

    public:
        static epoch_t try_commit(const transaction tx) noexcept
        {
            LSTM_ASSERT(tx.version() <= default_domain().get_clock());

            const thread_data& tls_td = tx.get_thread_data();
            if (tls_td.write_set.empty())
                return std::numeric_limits<epoch_t>::lowest(); // synchronize on the earliest epoch

            return slow_path(tx);
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_COMMIT_ALGORITHM_HPP */
#ifndef LSTM_DETAIL_TRANSACTION_LOG_HPP
#define LSTM_DETAIL_TRANSACTION_LOG_HPP

// clang-format off
#ifdef LSTM_LOG_TRANSACTIONS
    #include <lstm/detail/compiler.hpp>
    #include <lstm/detail/namespace_macros.hpp>

    #include <iomanip>
    #include <sstream>
    #include <string>
    #include <vector>
    
    #define LSTM_LOG_FAIL_TX()         ++lstm::detail::tls_record().failures
    #define LSTM_LOG_USER_FAIL_TX()    ++lstm::detail::tls_record().user_failures
    #define LSTM_LOG_SUCC_TX()         ++lstm::detail::tls_record().successes
    #define LSTM_LOG_BLOOM_COLLISION() ++lstm::detail::tls_record().bloom_collisions
    #define LSTM_LOG_BLOOM_SUCCESS()   ++lstm::detail::tls_record().bloom_successes
    #define LSTM_LOG_QUIESCENCE()      ++lstm::detail::tls_record().quiesce
    #define LSTM_LOG_READ_AND_WRITE_SET_SIZE(read_size, write_size)                                \
        lstm::detail::tls_record().bump_read_write(read_size, write_size)                          \
    /**/
    #define LSTM_LOG_PUBLISH_RECORD()                                                              \
        do {                                                                                       \
            lstm::detail::transaction_log::get().publish(lstm::detail::tls_record());              \
            lstm::detail::tls_record() = {};                                                       \
        } while(0)
    /**/
    #define LSTM_LOG_CLEAR() lstm::detail::transaction_log::get().clear()
    #ifndef LSTM_LOG_DUMP
        #include <iostream>
        #define LSTM_LOG_DUMP() (std::cout << lstm::detail::transaction_log::get().results())
    #endif /* LSTM_LOG_DUMP */
#else
    #define LSTM_LOG_FAIL_TX()                                      /**/
    #define LSTM_LOG_USER_FAIL_TX()                                 /**/
    #define LSTM_LOG_SUCC_TX()                                      /**/
    #define LSTM_LOG_BLOOM_COLLISION()                              /**/
    #define LSTM_LOG_BLOOM_SUCCESS()                                /**/
    #define LSTM_LOG_QUIESCENCE()                                   /**/
    #define LSTM_LOG_READ_AND_WRITE_SET_SIZE(read_size, write_size) /**/
    #define LSTM_LOG_PUBLISH_RECORD()                               /**/
    #define LSTM_LOG_CLEAR()                                        /**/
    #ifndef LSTM_LOG_DUMP
        #define LSTM_LOG_DUMP()                                     /**/
    #endif /* LSTM_LOG_DUMP */
#endif /* LSTM_LOG_TRANSACTIONS */
// clang-format on

#ifdef LSTM_LOG_TRANSACTIONS

LSTM_DETAIL_BEGIN
    struct thread_record
    {
        std::size_t user_failures{0};
        std::size_t failures{0};
        std::size_t successes{0};
        std::size_t bloom_collisions{0};
        std::size_t bloom_successes{0};
        std::size_t quiesce{0};
        std::size_t max_write_size{0};
        std::size_t max_read_size{0};
        double      avg_read_size{0.};
        double      avg_write_size{0.};

        thread_record() noexcept = default;

        inline void
        bump_read_write(const std::size_t in_read_size, const std::size_t in_write_size) noexcept
        {
            max_read_size  = std::max(max_read_size, in_read_size);
            max_write_size = std::max(max_write_size, in_write_size);
            avg_read_size
                = avg_read_size + (in_read_size - avg_read_size) / (total_transactions() + 1);
            avg_write_size
                = avg_write_size + (in_write_size - avg_write_size) / (total_transactions() + 1);
        }

        inline std::size_t internal_failures() const noexcept
        {
            LSTM_ASSERT(user_failures <= failures);
            return failures - user_failures;
        }

        inline std::size_t total_transactions() const noexcept { return failures + successes; }

        inline float success_rate() const noexcept
        {
            LSTM_ASSERT(successes <= total_transactions());
            return successes / float(total_transactions());
        }

        inline float failure_rate() const noexcept
        {
            LSTM_ASSERT(failures <= total_transactions());
            return failures / float(total_transactions());
        }

        inline float internal_failure_rate() const noexcept
        {
            LSTM_ASSERT(internal_failures() <= total_transactions());
            return internal_failures() / float(total_transactions());
        }

        inline float user_failure_rate() const noexcept
        {
            LSTM_ASSERT(user_failures <= total_transactions());
            return user_failures / float(total_transactions());
        }

        inline std::size_t total_bloom_checks() const noexcept
        {
            return bloom_collisions + bloom_successes;
        }

        inline float bloom_success_rate() const noexcept
        {
            LSTM_ASSERT(bloom_successes <= total_bloom_checks());
            return bloom_successes / float(total_bloom_checks());
        }

        inline float bloom_collision_rate() const noexcept
        {
            LSTM_ASSERT(bloom_collisions <= total_bloom_checks());
            return bloom_collisions / float(total_bloom_checks());
        }

        inline float quiesce_rate() const noexcept
        {
            LSTM_ASSERT(quiesce <= total_transactions());
            return quiesce / float(total_transactions());
        }

        std::string results() const
        {
            std::ostringstream ostr;
            ostr << "    Total Transactions:     " << total_transactions() << '\n'
                 << "    Total Successes:        " << successes << '\n'
                 << "    Total Failures:         " << failures << '\n'
                 << "    Internal Failures:      " << internal_failures() << '\n'
                 << "    User Failures:          " << user_failures << '\n'
                 << "    Success Rate:           " << success_rate() << '\n'
                 << "    Failure Rate:           " << failure_rate() << '\n'
                 << "    Internal Failure Rate:  " << internal_failure_rate() << '\n'
                 << "    User Failure Rate:      " << user_failure_rate() << '\n'
                 << "    Total Bloom Checks:     " << total_bloom_checks() << '\n'
                 << "    Bloom Collisions:       " << bloom_collisions << '\n'
                 << "    Bloom Successes:        " << bloom_successes << '\n'
                 << "    Bloom Collision Rate:   " << bloom_collision_rate() << '\n'
                 << "    Bloom Success Rate:     " << bloom_success_rate() << '\n'
                 << "    Quiesce Count:          " << quiesce << '\n'
                 << "    Quiesce Rate:           " << quiesce_rate() << '\n'
                 << "    Max Read Set Size:      " << max_read_size << '\n'
                 << "    Max Write Set Size:     " << max_write_size << '\n'
                 << "    Average Read Set Size:  " << avg_read_size << '\n'
                 << "    Average Write Set Size: " << avg_write_size << '\n';
            return ostr.str();
        }
    };

    inline thread_record& tls_record() noexcept
    {
        static LSTM_THREAD_LOCAL thread_record record{};
        return record;
    }

    struct transaction_log
    {
    private:
        using records_t          = std::vector<thread_record>;
        using records_iter       = typename records_t::iterator;
        using records_value_type = typename records_t::value_type;

        records_t records_;

        transaction_log()                       = default;
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;

        std::size_t
        total_count(std::function<std::size_t(const thread_record*)> accessor) const noexcept
        {
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result += accessor(&tid_record);
            return result;
        }

    public:
        static transaction_log& get() noexcept
        {
            static transaction_log singleton;
            return singleton;
        }

        inline void publish(thread_record record) noexcept
        {
            records_.emplace_back(std::move(record));
        }

        std::size_t total_transactions() const noexcept
        {
            return total_count(&thread_record::total_transactions);
        }

        std::size_t total_failures() const noexcept
        {
            return total_count(&thread_record::failures);
        }

        std::size_t total_successes() const noexcept
        {
            return total_count(&thread_record::successes);
        }

        std::size_t total_internal_failures() const noexcept
        {
            return total_count(&thread_record::internal_failures);
        }

        std::size_t total_user_failures() const noexcept
        {
            return total_count(&thread_record::user_failures);
        }

        std::size_t total_bloom_collisions() const noexcept
        {
            return total_count(&thread_record::bloom_collisions);
        }

        std::size_t total_bloom_successes() const noexcept
        {
            return total_count(&thread_record::bloom_successes);
        }

        std::size_t total_bloom_checks() const noexcept
        {
            return total_count(&thread_record::total_bloom_checks);
        }

        std::size_t total_quiesces() const noexcept { return total_count(&thread_record::quiesce); }

        inline float success_rate() const noexcept
        {
            return total_successes() / float(total_transactions());
        }

        inline float failure_rate() const noexcept
        {
            return total_failures() / float(total_transactions());
        }

        inline float internal_failure_rate() const noexcept
        {
            return total_internal_failures() / float(total_transactions());
        }

        inline float user_failure_rate() const noexcept
        {
            return total_user_failures() / float(total_transactions());
        }

        inline float bloom_success_rate() const noexcept
        {
            return total_bloom_successes() / float(total_bloom_checks());
        }

        inline float bloom_collision_rate() const noexcept
        {
            return total_bloom_collisions() / float(total_bloom_checks());
        }

        inline float quiesce_rate() const noexcept
        {
            return total_quiesces() / float(total_transactions());
        }

        inline std::size_t max_read_size() const noexcept
        {
            std::size_t result = 0;
            for (auto& record : records_)
                result = std::max(result, record.max_read_size);
            return result;
        }

        inline std::size_t max_write_size() const noexcept
        {
            std::size_t result = 0;
            for (auto& record : records_)
                result = std::max(result, record.max_write_size);
            return result;
        }

        inline double avg_read_size() const noexcept
        {
            double result = 0.f;
            for (auto& record : records_)
                result += record.avg_read_size * record.total_transactions() / total_transactions();
            return result;
        }

        inline double avg_write_size() const noexcept
        {
            double result = 0.f;
            for (auto& record : records_)
                result
                    += record.avg_write_size * record.total_transactions() / total_transactions();
            return result;
        }

        std::size_t thread_count() const noexcept { return records_.size(); }

        const records_t& records() const noexcept { return records_; }

        void clear() noexcept { records_.clear(); }

        std::string results(bool per_thread = true) const
        {
            std::ostringstream ostr;
            ostr << "Total Transactions:      " << total_transactions() << '\n'
                 << "Thread count:            " << thread_count() << '\n'
                 << "Total Successes:         " << total_successes() << '\n'
                 << "Total Failures:          " << total_failures() << '\n'
                 << "Total Internal Failures: " << total_internal_failures() << '\n'
                 << "Total User Failures:     " << total_user_failures() << '\n'
                 << "Success Rate:            " << success_rate() << '\n'
                 << "Failure Rate:            " << failure_rate() << '\n'
                 << "Internal Failure Rate:   " << internal_failure_rate() << '\n'
                 << "User Failure Rate:       " << user_failure_rate() << '\n'
                 << "Total Bloom Checks:      " << total_bloom_checks() << '\n'
                 << "Bloom Collisions:        " << total_bloom_collisions() << '\n'
                 << "Bloom Successes:         " << total_bloom_successes() << '\n'
                 << "Bloom Collision Rate:    " << bloom_collision_rate() << '\n'
                 << "Bloom Success Rate:      " << bloom_success_rate() << '\n'
                 << "Quiesce Count:           " << total_quiesces() << '\n'
                 << "Quiesce Rate:            " << quiesce_rate() << '\n'
                 << "Max Read Set Size:       " << max_read_size() << '\n'
                 << "Max Write Set Size:      " << max_write_size() << '\n'
                 << "Average Read Set Size:   " << avg_read_size() << '\n'
                 << "Average Write Set Size:  " << avg_write_size() << "\n\n";

            if (per_thread) {
                std::size_t i = 0;
                for (auto& record : records_) {
                    ostr << "--== Thread: " << std::setw(4) << i++ << " ==--" << '\n';
                    ostr << record.results() << '\n';
                }
            }
            return ostr.str();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_LOG_TRANSACTIONS */

#endif /* LSTM_DETAIL_TRANSACTION_LOG_HPP */
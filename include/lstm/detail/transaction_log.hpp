#ifndef LSTM_DETAIL_TRANSACTION_LOG_HPP
#define LSTM_DETAIL_TRANSACTION_LOG_HPP

// clang-format off
#ifdef LSTM_LOG_ON
    #include <lstm/detail/compiler.hpp>
    #include <lstm/detail/namespace_macros.hpp>

    #include <iomanip>
    #include <sstream>
    #include <string>
    #include <vector>
    
    #define LSTM_LOG_READS(amt)          lstm::detail::tls_record().reads += amt
    #define LSTM_LOG_WRITES(amt)         lstm::detail::tls_record().writes += amt
    #define LSTM_LOG_MAX_READ_SIZE(amt)  lstm::detail::tls_record().max_read_size = std::max(lstm::detail::tls_record().max_read_size, static_cast<std::uint64_t>(amt))
    #define LSTM_LOG_MAX_WRITE_SIZE(amt) lstm::detail::tls_record().max_write_size = std::max(lstm::detail::tls_record().max_write_size, static_cast<std::uint64_t>(amt))
    #define LSTM_LOG_QUIESCES()          ++lstm::detail::tls_record().quiesces
    #define LSTM_LOG_USER_FAILURES()     ++lstm::detail::tls_record().user_failures
    #define LSTM_LOG_FAILURES()          ++lstm::detail::tls_record().failures
    #define LSTM_LOG_SUCCESSES()         ++lstm::detail::tls_record().successes
    #define LSTM_LOG_BLOOM_COLLISIONS()  ++lstm::detail::tls_record().bloom_collisions
    #define LSTM_LOG_BLOOM_SUCCESSES()   ++lstm::detail::tls_record().bloom_successes
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
    #define LSTM_LOG_READS(amt)          /**/
    #define LSTM_LOG_WRITES(amt)         /**/
    #define LSTM_LOG_MAX_READ_SIZE(amt)  /**/
    #define LSTM_LOG_MAX_WRITE_SIZE(amt) /**/
    #define LSTM_LOG_QUIESCES()          /**/
    #define LSTM_LOG_USER_FAILURES()     /**/
    #define LSTM_LOG_FAILURES()          /**/
    #define LSTM_LOG_SUCCESSES()         /**/
    #define LSTM_LOG_BLOOM_COLLISIONS()  /**/
    #define LSTM_LOG_BLOOM_SUCCESSES()   /**/
    #define LSTM_LOG_PUBLISH_RECORD()                               /**/
    #define LSTM_LOG_CLEAR()                                        /**/
    #ifndef LSTM_LOG_DUMP
        #define LSTM_LOG_DUMP()                                     /**/
    #endif /* LSTM_LOG_DUMP */
#endif /* LSTM_LOG_ON */
// clang-format on

#ifdef LSTM_LOG_ON

LSTM_DETAIL_BEGIN
    struct thread_record
    {
        std::uint64_t reads{0};
        std::uint64_t writes{0};
        std::uint64_t max_read_size{0};
        std::uint64_t max_write_size{0};
        std::uint64_t quiesces{0};
        std::uint64_t user_failures{0};
        std::uint64_t failures{0};
        std::uint64_t successes{0};
        std::uint64_t bloom_collisions{0};
        std::uint64_t bloom_successes{0};

        thread_record() noexcept = default;

        auto internal_failures() const noexcept { return failures - user_failures; }
        auto transactions() const noexcept { return failures + successes; }
        auto success_rate() const noexcept { return successes / float(transactions()); }
        auto failure_rate() const noexcept { return failures / float(transactions()); }
        auto internal_failure_rate() const noexcept
        {
            return internal_failures() / float(transactions());
        }
        auto user_failure_rate() const noexcept { return user_failures / float(transactions()); }
        auto bloom_checks() const noexcept { return bloom_successes + bloom_collisions; }
        auto bloom_collision_rate() const noexcept
        {
            return bloom_collisions / float(bloom_checks());
        }
        auto quiesce_rate() const noexcept { return quiesces / float(transactions()); }
        auto average_read_size() const noexcept { return reads / float(transactions()); }
        auto average_write_size() const noexcept { return writes / float(transactions()); }

        std::string results() const
        {
            std::ostringstream ostr;
            ostr << "    Transactions:          " << transactions() << '\n'
                 << "    Success Rate:          " << success_rate() << '\n'
                 << "    Failure Rate:          " << failure_rate() << '\n'
                 << "    Quiesce Rate:          " << quiesce_rate() << '\n'
                 << "    Average Write Size:    " << average_write_size() << '\n'
                 << "    Average Read Size:     " << average_read_size() << '\n'
                 << "    Max Write Size:        " << max_write_size << '\n'
                 << "    Max Read Size:         " << max_read_size << '\n'
                 << "    Bloom Collision Rate:  " << bloom_collision_rate() << '\n'
                 << "    Reads:                 " << reads << '\n'
                 << "    Writes:                " << writes << '\n'
                 << "    Quiesces:              " << quiesces << '\n'
                 << "    Successes:             " << successes << '\n'
                 << "    Failures:              " << failures << '\n'
                 << "    Internal Failure Rate: " << internal_failure_rate() << '\n'
                 << "    User Failure Rate:     " << user_failure_rate() << '\n'
                 << "    Internal Failures:     " << internal_failures() << '\n'
                 << "    User Failures:         " << user_failures << '\n'
                 << "    Bloom Collisions:      " << bloom_collisions << '\n'
                 << "    Bloom Successes:       " << bloom_successes << '\n'
                 << "    Bloom Checks:          " << bloom_checks() << '\n';
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

        std::uint64_t
        total_count(std::function<std::size_t(const thread_record*)> accessor) const noexcept
        {
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result += accessor(&tid_record);
            return result;
        }

        std::uint64_t max(std::function<std::size_t(const thread_record*)> accessor) const noexcept
        {
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result = std::max(result, accessor(&tid_record));
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

        auto reads() const noexcept { return total_count(&thread_record::reads); }
        auto writes() const noexcept { return total_count(&thread_record::writes); }
        auto max_read_size() const noexcept { return this->max(&thread_record::max_read_size); }
        auto max_write_size() const noexcept { return this->max(&thread_record::max_write_size); }
        auto quiesces() const noexcept { return total_count(&thread_record::quiesces); }
        auto user_failures() const noexcept { return total_count(&thread_record::user_failures); }
        auto failures() const noexcept { return total_count(&thread_record::failures); }
        auto successes() const noexcept { return total_count(&thread_record::successes); }
        auto bloom_collisions() const noexcept
        {
            return total_count(&thread_record::bloom_collisions);
        }
        auto bloom_successes() const noexcept
        {
            return total_count(&thread_record::bloom_successes);
        }
        auto internal_failures() const noexcept { return failures() - user_failures(); }
        auto transactions() const noexcept { return failures() + successes(); }
        auto success_rate() const noexcept { return successes() / float(transactions()); }
        auto failure_rate() const noexcept { return failures() / float(transactions()); }
        auto internal_failure_rate() const noexcept
        {
            return internal_failures() / float(transactions());
        }
        auto user_failure_rate() const noexcept { return user_failures() / float(transactions()); }
        auto bloom_checks() const noexcept { return bloom_successes() + bloom_collisions(); }
        auto bloom_collision_rate() const noexcept
        {
            return bloom_collisions() / float(bloom_checks());
        }
        auto quiesce_rate() const noexcept { return quiesces() / float(transactions()); }
        auto average_read_size() const noexcept { return reads() / float(transactions()); }
        auto average_write_size() const noexcept { return writes() / float(transactions()); }

        std::size_t thread_count() const noexcept { return records_.size(); }

        const records_t& records() const noexcept { return records_; }

        void clear() noexcept { records_.clear(); }

        std::string results(bool per_thread = true) const
        {
            std::ostringstream ostr;
            ostr << "Transactions:          " << transactions() << '\n'
                 << "Success Rate:          " << success_rate() << '\n'
                 << "Failure Rate:          " << failure_rate() << '\n'
                 << "Quiesce Rate:          " << quiesce_rate() << '\n'
                 << "Average Write Size:    " << average_write_size() << '\n'
                 << "Average Read Size:     " << average_read_size() << '\n'
                 << "Max Write Size:        " << max_write_size() << '\n'
                 << "Max Read Size:         " << max_read_size() << '\n'
                 << "Bloom Collision Rate:  " << bloom_collision_rate() << '\n'
                 << "Reads:                 " << reads() << '\n'
                 << "Writes:                " << writes() << '\n'
                 << "Quiesces:              " << quiesces() << '\n'
                 << "Successes:             " << successes() << '\n'
                 << "Failures:              " << failures() << '\n'
                 << "Internal Failure Rate: " << internal_failure_rate() << '\n'
                 << "User Failure Rate:     " << user_failure_rate() << '\n'
                 << "Internal Failures:     " << internal_failures() << '\n'
                 << "User Failures:         " << user_failures() << '\n'
                 << "Bloom Collisions:      " << bloom_collisions() << '\n'
                 << "Bloom Successes:       " << bloom_successes() << '\n'
                 << "Bloom Checks:          " << bloom_checks() << '\n';

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

#endif /* LSTM_LOG_ON */

#endif /* LSTM_DETAIL_TRANSACTION_LOG_HPP */

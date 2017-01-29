#ifndef LSTM_DETAIL_TRANSACTION_LOG_HPP
#define LSTM_DETAIL_TRANSACTION_LOG_HPP

#include <cassert>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

LSTM_DETAIL_BEGIN
    struct thread_record {
        std::size_t user_failures;
        std::size_t internal_failures;
        std::size_t successes;
        std::size_t bloom_collisions;
        std::size_t bloom_successes;
        
        constexpr inline thread_record() noexcept
            : user_failures{0}
            , internal_failures{0}
            , successes{0}
            , bloom_collisions{0}
            , bloom_successes{0}
        {}
        
        inline constexpr std::size_t total_failures() const noexcept
        { return user_failures + internal_failures; }
        
        inline constexpr std::size_t total_transactions() const noexcept
        { return total_failures() + successes; }
        
        inline constexpr float success_rate() const noexcept
        { return successes / float(total_transactions()); }
        
        inline constexpr float failure_rate() const noexcept
        { return total_failures() / float(total_transactions()); }
        
        inline constexpr float internal_failure_rate() const noexcept
        { return internal_failures / float(total_transactions()); }
        
        inline constexpr float user_failure_rate() const noexcept
        { return user_failures / float(total_transactions()); }
        
        inline constexpr std::size_t total_bloom_checks() const noexcept
        { return bloom_collisions + bloom_successes; }
        
        inline constexpr float bloom_success_rate() const noexcept
        { return bloom_successes / float(total_bloom_checks()); }
        
        inline constexpr float bloom_collision_rate() const noexcept
        { return bloom_collisions / float(total_bloom_checks()); }
        
        std::string results() const {
            std::ostringstream ostr;
            ostr << "    Total Transactions:    " << total_transactions() << '\n'
                 << "    Total Successes:       " << successes << '\n'
                 << "    Total Failures:        " << total_failures() << '\n'
                 << "    Internal Failures:     " << internal_failures << '\n'
                 << "    User Failures:         " << user_failures << '\n'
                 << "    Success Rate:          " << success_rate() << '\n'
                 << "    Failure Rate:          " << failure_rate() << '\n'
                 << "    Internal Failure Rate: " << internal_failure_rate() << '\n'
                 << "    User Failure Rate:     " << user_failure_rate() << '\n'
                 << "    Total Bloom Checks:    " << total_bloom_checks() << '\n'
                 << "    Bloom Collisions:      " << bloom_collisions << '\n'
                 << "    Bloom Successes:       " << bloom_successes << '\n'
                 << "    Bloom Collision Rate:  " << bloom_collision_rate() << '\n'
                 << "    Bloom Success Rate:    " << bloom_success_rate() << '\n'
                 ;
            return ostr.str();
        }
    };

    struct transaction_log {
    private:
        using records_t = std::map<std::thread::id, thread_record>;
        using records_iter = typename records_t::iterator;
        using records_value_type = typename records_t::value_type;
        
        records_t records_;
        std::mutex records_mut;
        
        transaction_log() = default;
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;
        
        records_iter find_or_register()
        { return records_.find(std::this_thread::get_id()); }
        
        std::size_t
        total_count(std::function<std::size_t(const thread_record*)> accessor) const noexcept {
            std::size_t result = 0;
            for (auto& tid_record : records_)
                result += accessor(&tid_record.second);
            return result;
        }
        
    public:
        static transaction_log& get() {
            static transaction_log singleton;
            return singleton;
        }
        
        inline records_iter register_thread(const std::thread::id& id) noexcept {
            auto iter_success = records_.emplace(id, thread_record());
            assert(iter_success.second);
            return iter_success.first;
        }
        
        void add_success() noexcept {
            auto iter = find_or_register();
            ++iter->second.successes;
        }
        
        void add_internal_failure() noexcept {
            auto iter = find_or_register();
            ++iter->second.internal_failures;
        }
        
        void add_user_failure() noexcept {
            auto iter = find_or_register();
            ++iter->second.user_failures;
        }
        
        void add_bloom_success() noexcept {
            auto iter = find_or_register();
            ++iter->second.bloom_successes;
        }
        
        void add_bloom_collision() noexcept {
            auto iter = find_or_register();
            ++iter->second.bloom_collisions;
        }
        
        std::size_t total_transactions() const noexcept
        { return total_count(&thread_record::total_transactions); }
        
        std::size_t total_failures() const noexcept
        { return total_count(&thread_record::internal_failures); }
        
        std::size_t total_successes() const noexcept
        { return total_count(&thread_record::successes); }
        
        std::size_t total_internal_failures() const noexcept
        { return total_count(&thread_record::internal_failures); }
        
        std::size_t total_user_failures() const noexcept
        { return total_count(&thread_record::user_failures); }
        
        std::size_t total_bloom_collisions() const noexcept
        { return total_count(&thread_record::bloom_collisions); }
        
        std::size_t total_bloom_successes() const noexcept
        { return total_count(&thread_record::bloom_successes); }
        
        std::size_t total_bloom_checks() const noexcept
        { return total_count(&thread_record::total_bloom_checks); }
        
        inline float success_rate() const noexcept
        { return total_successes() / float(total_transactions()); }
        
        inline float failure_rate() const noexcept
        { return total_failures() / float(total_transactions()); }
        
        inline float internal_failure_rate() const noexcept
        { return total_internal_failures() / float(total_transactions()); }
        
        inline float user_failure_rate() const noexcept
        { return total_user_failures() / float(total_transactions()); }
        
        inline float bloom_success_rate() const noexcept
        { return total_bloom_successes() / float(total_bloom_checks()); }
        
        inline float bloom_collision_rate() const noexcept
        { return total_bloom_collisions() / float(total_bloom_checks()); }
        
        std::size_t thread_count() const noexcept { return records_.size(); }
        
        const records_t& records() const noexcept { return records_; }
        
        void clear() noexcept { records_.clear(); }
        
        bool each_threads_successes_equals(std::size_t count) const noexcept {
            return std::find_if(std::begin(records_),
                                std::end(records_),
                                [count](const records_value_type& thread_record) noexcept -> bool
                                { return thread_record.second.successes != count; })
                == std::end(records_);
        }
        
        std::string results(bool per_thread = true) const {
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
                 << "Bloom Success Rate:      " << bloom_success_rate() <<  "\n\n"
                 ;
                 
            if (per_thread) {
                std::size_t i = 0;
                for (auto& record : records_) {
                    ostr << "--== Thread: "<< std::setw(4) << i++ << " ==--" << '\n';
                    ostr << record.second.results() << '\n';
                }
            }
            return ostr.str();
        }
    };
LSTM_DETAIL_END

#endif /*LSTM_DETAIL_TRANSACTION_LOG_HPP*/
#ifndef LSTM_TEST_STATSD_CLIENT_HPP
#define LSTM_TEST_STATSD_CLIENT_HPP

#include <lstm/lstm.hpp>

#if defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG)

extern "C" {
#include <statsd-client.h>
}

struct statsd_client
{
private:
    statsd_link* link;

    statsd_client()
    {
        link = statsd_init("127.0.0.1", 8125);
        LSTM_ASSERT(link);
    }
    ~statsd_client() { statsd_finalize(link); }

    static statsd_client& get()
    {
        static statsd_client client;
        return client;
    }

public:
    LSTM_NOINLINE static void dump_log()
    {
        statsd_link*              link  = get().link;
        lstm::detail::perf_stats& stats = lstm::detail::perf_stats::get();
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.transactions"),
                     stats.transactions());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.success_rate"),
                      stats.success_rate());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.failure_rate"),
                      stats.failure_rate());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.quiesce_rate"),
                      stats.quiesce_rate());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.backoff_rate"),
                      stats.backoff_rate());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.average_write_size"),
                      stats.average_write_size());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.average_read_size"),
                      stats.average_read_size());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.max_write_size"),
                     stats.max_write_size());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.max_read_size"),
                     stats.max_read_size());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.bloom_collision_rate"),
                      stats.bloom_collision_rate());
        statsd_gauge(link, const_cast<char*>(LSTM_TESTNAME ".process.reads"), stats.reads());
        statsd_gauge(link, const_cast<char*>(LSTM_TESTNAME ".process.writes"), stats.writes());
        statsd_gauge(link, const_cast<char*>(LSTM_TESTNAME ".process.quiesces"), stats.quiesces());
        statsd_gauge(link, const_cast<char*>(LSTM_TESTNAME ".process.backoffs"), stats.backoffs());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.successes"),
                     stats.successes());
        statsd_gauge(link, const_cast<char*>(LSTM_TESTNAME ".process.failures"), stats.failures());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.internal_failure_rate"),
                      stats.internal_failure_rate());
        statsd_gauged(link,
                      const_cast<char*>(LSTM_TESTNAME ".process.user_failure_rate"),
                      stats.user_failure_rate());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.internal_failures"),
                     stats.internal_failures());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.user_failures"),
                     stats.user_failures());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.bloom_collisions"),
                     stats.bloom_collisions());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.bloom_successes"),
                     stats.bloom_successes());
        statsd_gauge(link,
                     const_cast<char*>(LSTM_TESTNAME ".process.bloom_checks"),
                     stats.bloom_checks());

        int i = 0;
        for (auto& record : stats.records()) {
            {
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".transactions")
                                     .c_str()),
                             record.transactions());
                statsd_gauged(link,
                              const_cast<char*>(
                                  (LSTM_TESTNAME ".thread" + std::to_string(i) + ".success_rate")
                                      .c_str()),
                              record.success_rate());
                statsd_gauged(link,
                              const_cast<char*>(
                                  (LSTM_TESTNAME ".thread" + std::to_string(i) + ".failure_rate")
                                      .c_str()),
                              record.failure_rate());
                statsd_gauged(link,
                              const_cast<char*>(
                                  (LSTM_TESTNAME ".thread" + std::to_string(i) + ".quiesce_rate")
                                      .c_str()),
                              record.quiesce_rate());
                statsd_gauged(link,
                              const_cast<char*>(
                                  (LSTM_TESTNAME ".thread" + std::to_string(i) + ".backoff_rate")
                                      .c_str()),
                              record.backoff_rate());
                statsd_gauged(link,
                              const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                 + ".average_write_size")
                                                    .c_str()),
                              record.average_write_size());
                statsd_gauged(link,
                              const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                 + ".average_read_size")
                                                    .c_str()),
                              record.average_read_size());
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".max_write_size")
                                     .c_str()),
                             record.max_write_size);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".max_read_size")
                                     .c_str()),
                             record.max_read_size);
                statsd_gauged(link,
                              const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                 + ".bloom_collision_rate")
                                                    .c_str()),
                              record.bloom_collision_rate());
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".reads").c_str()),
                             record.reads);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".writes").c_str()),
                             record.writes);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".quiesces")
                                     .c_str()),
                             record.quiesces);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".backoffs")
                                     .c_str()),
                             record.backoffs);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".successes")
                                     .c_str()),
                             record.successes);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".failures")
                                     .c_str()),
                             record.failures);
                statsd_gauged(link,
                              const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                 + ".internal_failure_rate")
                                                    .c_str()),
                              record.internal_failure_rate());
                statsd_gauged(link,
                              const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                 + ".user_failure_rate")
                                                    .c_str()),
                              record.user_failure_rate());
                statsd_gauge(link,
                             const_cast<char*>((LSTM_TESTNAME ".thread" + std::to_string(i)
                                                + ".internal_failures")
                                                   .c_str()),
                             record.internal_failures());
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".user_failures")
                                     .c_str()),
                             record.user_failures);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".bloom_collisions")
                                     .c_str()),
                             record.bloom_collisions);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".bloom_successes")
                                     .c_str()),
                             record.bloom_successes);
                statsd_gauge(link,
                             const_cast<char*>(
                                 (LSTM_TESTNAME ".thread" + std::to_string(i) + ".bloom_checks")
                                     .c_str()),
                             record.bloom_checks());
                ++i;
            }
        }
    }
};

#else /* defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG) */

struct statsd_client
{
    static constexpr void dump_log() noexcept {}
};

#endif /* defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG) */

#endif /* LSTM_TEST_STATSD_CLIENT_HPP */
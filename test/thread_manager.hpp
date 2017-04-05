#ifndef LSTM_TEST_THREAD_MANAGER_HPP
#define LSTM_TEST_THREAD_MANAGER_HPP

#include <lstm/thread_data.hpp>

#include "statsd_client.hpp"

// clang-format off
#ifndef LSTM_USE_BOOST_FIBERS
    #include <thread>
    using thread_t = std::thread;
#else
    #include <boost/fiber/fiber.hpp>
    using thread_t = boost::fibers::fiber;
#endif
// clang-format on

#include <vector>

// for tests only
struct thread_manager
{
private:
    std::vector<thread_t> threads;
    std::atomic<bool>     _run{false};

public:
    template<typename F>
    void queue_thread(F&& f)
    {
        using namespace std::chrono_literals;
        threads.emplace_back([ this, f = (F &&) f ] {
            lstm::tls_thread_data(); // initialize the thread data
            while (!_run.load(LSTM_RELAXED))
                LSTM_THIS_CONTEXT::sleep_for(1ns);
            f();
        });
    }

    template<typename F>
    void queue_loop_n(F&& f, const int n)
    {
        using namespace std::chrono_literals;
        threads.emplace_back([ this, f = (F &&) f, n ] {
            lstm::tls_thread_data(); // initialize the thread data
            while (!_run.load(LSTM_RELAXED))
                LSTM_THIS_CONTEXT::sleep_for(1ns);

            for (int i = 0; i < n; ++i)
                f();
        });
    }

    void join_threads()
    {
        for (auto& thread : threads)
            thread.join();
    }

    void clear()
    {
        threads.clear();
        _run.store(false, LSTM_RELAXED);
    }

    void join_and_clear_threads()
    {
        join_threads();
        clear();
    }

    void start_threads() { _run.store(true, LSTM_RELAXED); }

    void run()
    {
        start_threads();
        join_and_clear_threads();
    }

    ~thread_manager()
    {
#if defined(LSTM_STATSD_AVAILABLE) && defined(LSTM_PERF_STATS_ON) && defined(NDEBUG)
        statsd_client::dump_log();
#else
        LSTM_PERF_STATS_DUMP();
#endif
        LSTM_PERF_STATS_CLEAR();
    }
};

#endif /* LSTM_TEST_THREAD_MANAGER_HPP */
#ifndef LSTM_TEST_THREAD_MANAGER_HPP
#define LSTM_TEST_THREAD_MANAGER_HPP

#include <thread>
#include <vector>

// for tests only
struct thread_manager {
private:
    std::vector<std::thread> threads;
    std::atomic<bool> _run{false};
    
public:
    template<typename F>
    void queue_thread(F&& f) {
        threads.emplace_back([this, f = (F&&)f]{
            while(!_run.load(LSTM_ACQUIRE));
            f();
        });
        LSTM_LOG_REGISTER_THREAD_ID(threads.back().get_id());
    }
    
    template<typename F>
    void queue_loop_n(F&& f, const int n) {
        threads.emplace_back([this, f = (F&&)f, n]{
            while(!_run.load(LSTM_ACQUIRE));
            for (int i = 0; i < n; ++i)
                f();
        });
        LSTM_LOG_REGISTER_THREAD_ID(threads.back().get_id());
    }
    
    void join_threads() {
        for (auto& thread : threads)
            thread.join();
    }
    
    void clear() {
        threads.clear();
        _run.store(false, LSTM_RELEASE);
    }
    
    void join_and_clear_threads() {
        join_threads();
        clear();
    }
    
    void start_threads() { _run.store(true, LSTM_RELEASE); }
    
    void run() {
        start_threads();
        join_and_clear_threads();
    }
};

#endif /* LSTM_TEST_THREAD_MANAGER_HPP */
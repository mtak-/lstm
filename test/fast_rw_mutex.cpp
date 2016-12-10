#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

static constexpr auto loop_count0 = LSTM_TEST_INIT(10000000, 10000000);

int main() {
    lstm::detail::fast_rw_mutex mut;
    
    thread_manager manager;
    
    manager.queue_thread([&] {
        for (int i = 0; i < loop_count0; ++i) {
            mut.lock();
            mut.unlock();
        }
    });
    
    manager.queue_thread([&] {
        for (int i = 0; i < loop_count0; ++i) {
            mut.lock();
            mut.unlock();
        }
    });
    
    manager.queue_thread([&] {
        for (int i = 0; i < loop_count0; ++i) {
            mut.lock_shared();
            mut.unlock_shared();
        }
    });
    
    manager.queue_thread([&] {
        for (int i = 0; i < loop_count0; ++i) {
            mut.lock_shared();
            mut.unlock_shared();
        }
    });
    
    manager.run();
    
    return test_result();
}
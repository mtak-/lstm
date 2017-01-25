#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

#include <random>

static constexpr auto loop_count0 = LSTM_TEST_INIT(5000000, 200000);

using lstm::uword;

int main() {
    thread_manager manager;
    std::atomic<lstm::gp_t> gp{0};
    
    for (int j = 0; j < 5; ++j) {
        manager.queue_thread([&] {
            auto& tls_td = lstm::tls_thread_data();
            for (uword i = 0; i < loop_count0; ++i) {
                tls_td.access_lock(gp.load(LSTM_RELAXED));
                tls_td.access_unlock();
                tls_td.synchronize(gp.fetch_add(1, LSTM_RELAXED));
            }
        });
    }
    
    manager.run();
    
    return test_result();
}

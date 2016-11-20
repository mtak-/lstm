#include <lstm/easy_var.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

static constexpr auto loop_count0 = LSTM_TEST_INIT(5000000, 50000);
static constexpr auto loop_count1 = LSTM_TEST_INIT(100, 100);

using lstm::atomic;
using lstm::var;
using lstm::uword;

static var<uword> x{0};

int main() {
    {
        thread_manager manager;
        
        for (int j = 0; j < 5; ++j) {
            manager.queue_thread([] {
                auto& tls_td = lstm::detail::tls_thread_data();
                for (uword i = 0; i < loop_count0; ++i) {
                    tls_td.access_lock();
                    tls_td.access_unlock();
                    lstm::detail::synchronize();
                }
            });
        }
        
        manager.run();
    }
    {
        thread_manager manager;
        
        for (int j = 0; j < 3; ++j) {
            manager.queue_thread([] {
                for (uword i = 0; i < loop_count1; ++i) {
                    lstm::detail::thread_data_mut<>.lock_shared();
                    std::this_thread::sleep_for(std::chrono::milliseconds(12));
                    lstm::detail::thread_data_mut<>.unlock_shared();
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                }
            });
        }
        manager.queue_thread([] {
            for (uword i = 0; i < loop_count1; ++i) {
                lstm::detail::thread_data_mut<>.lock();
                std::this_thread::sleep_for(std::chrono::milliseconds(18));
                lstm::detail::thread_data_mut<>.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(21));
            }
        });
        
        manager.run();
    }

    return test_result();
}

#include <lstm/easy_var.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

#include <random>

static constexpr auto loop_count0 = LSTM_TEST_INIT(5000000, 50000);
static constexpr auto loop_count1 = LSTM_TEST_INIT(100, 100);

using lstm::uword;

static std::mt19937& e(int j) {
    static LSTM_THREAD_LOCAL std::mt19937 e(j);
    return e;
}

int main() {
    auto s = std::random_device{}();
    static LSTM_THREAD_LOCAL std::uniform_int_distribution<int> d(0,30);
    
    std::cout << "Seed: " << s << std::endl;
    
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
        
        for (int j = 0; j < 5; ++j) {
            manager.queue_thread([=] {
                auto& tls_td = lstm::detail::tls_thread_data();
                for (uword i = 0; i < loop_count1; ++i) {
                    tls_td.access_lock();
                    sleep_ms(d(e(j + s)));
                    tls_td.access_unlock();
                    sleep_ms(d(e(j + s)));
                    lstm::detail::synchronize();
                    sleep_ms(d(e(j + s)));
                }
            });
        }
        
        manager.run();
    }
    {
        thread_manager manager;
        
        for (int j = 0; j < 3; ++j) {
            manager.queue_thread([=] {
                for (uword i = 0; i < loop_count1; ++i) {
                    lstm::detail::thread_data_mut<>.lock_shared();
                    sleep_ms(d(e(j + s)));
                    lstm::detail::thread_data_mut<>.unlock_shared();
                    sleep_ms(d(e(j + s)));
                }
            });
        }
        manager.queue_thread([=] {
            for (uword i = 0; i < loop_count1; ++i) {
                lstm::detail::thread_data_mut<>.lock();
                sleep_ms(d(e(3 + s)));
                lstm::detail::thread_data_mut<>.unlock();
                sleep_ms(d(e(3 + s)));
            }
        });
        
        manager.run();
    }

    return test_result();
}

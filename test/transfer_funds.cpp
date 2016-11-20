#include <lstm/easy_var.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

static constexpr auto loop_count0 = LSTM_TEST_INIT(5000000, 50000);
static constexpr auto loop_count1 = LSTM_TEST_INIT(666666 * 5 + 4, 6666 * 5 + 4);

using lstm::atomic;
using lstm::easy_var;

static easy_var<int, debug_alloc<int>> account0{300};
static easy_var<int, debug_alloc<int>> account1{300};

int main() {
    {
        thread_manager manager;
        
        manager.queue_thread([] {
            for (int i = 0; i < loop_count0; ++i) {
                lstm::atomic([] {
                    if (account1 >= 20) {
                        account0 += 20;
                        account1 -= 20;
                    } else
                        lstm::retry();
                });
            }
        });
        
        manager.queue_thread([] {
            for (int i = 0; i < loop_count1; ++i) {
                lstm::atomic([] {
                    if (account0 >= 30) {
                        account1 += 30;
                        account0 -= 30;
                    } else
                        lstm::retry();
                });
            }
        });
        
        manager.run();
        
        CHECK(account0.unsafe_load() == 280);
        CHECK(account1.unsafe_load() == 320);
    }
    CHECK(debug_live_allocations<> == 0);
    
    return test_result();
}
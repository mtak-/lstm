#include <lstm/easy_var.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::easy_var;

static easy_var<int, debug_alloc<int>> account0{300};
static easy_var<int, debug_alloc<int>> account1{300};

int main() {
    thread_manager manager;
    
    manager.queue_thread([] {
        for (int i = 0; i < 5'000'000; ++i) {
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
        for (int i = 0; i < 666666 * 5 + 4; ++i) {
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
    
    CHECK(account0.unsafe() == 280);
    CHECK(account1.unsafe() == 320);
    CHECK(debug_live_allocations<> == 0);
    
    LSTM_LOG_DUMP();
    
    return test_result();
}
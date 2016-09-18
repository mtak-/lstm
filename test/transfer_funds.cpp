#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static var<int> account0{300};
static var<int> account1{300};

int main() {
    thread_manager manager;
    
    manager.queue_thread([] {
        for (int i = 0; i < 1000000; ++i) {
            lstm::atomic([](auto& tx) {
                tx.store(account0, tx.load(account0) + 20);
                tx.store(account1, tx.load(account1) - 20);
            });
        }
    });
    
    manager.queue_thread([] {
        for (int i = 0; i < 666666; ++i) {
            lstm::atomic([](auto& tx) {
                tx.store(account1, tx.load(account1) + 30);
                tx.store(account0, tx.load(account0) - 30);
            });
        }
    });
    
    manager.run();
    
    CHECK(account0.unsafe() == 320);
    CHECK(account1.unsafe() == 280);
    
    LSTM_LOG_DUMP();
    
    return test_result();
}
#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static constexpr auto loop_count = LSTM_TEST_INIT(200000, 40000);

int main() {
    var<int> x{0};
    
    {
        thread_manager tm;
        
        for (int i = 0; i < 5; ++i) {
            tm.queue_thread([&] {
                for (int j = 0; j < loop_count; ++j) {
                    try {
                        atomic([&](auto& tx) {
                            int foo = tx.load(x);
                            tx.store(x, foo + 5);
                            if (foo + 5 >= 10000)
                                throw std::exception{};
                        });
                    } catch(const std::exception&) {}
                }
            });
        }
        
        tm.run();
    }
    
    CHECK(x.unsafe_load() == 10000 - 5);
    
    return test_result();
}

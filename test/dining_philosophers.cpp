#include <lstm/lstm.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::read_write;
using lstm::var;

static constexpr std::size_t food_size = LSTM_TEST_INIT(10000, 10000);
static constexpr std::size_t repeat_count = LSTM_TEST_INIT(300, 30);

struct LSTM_CACHE_ALIGNED philosopher {
    LSTM_CACHE_ALIGNED std::size_t food{food_size};
};

struct fork {
    LSTM_CACHE_ALIGNED var<bool, debug_alloc<bool>> in_use{false};
};

static_assert(decltype(fork{}.in_use)::atomic == true, "");

auto get_loop(philosopher& p, fork& f0, fork& f1) {
    return [&] {
        while (p.food != 0) {
            read_write([&](auto& tx) {
                if (!tx.read(f0.in_use) && !tx.read(f1.in_use)) {
                    tx.write(f0.in_use, true);
                    tx.write(f1.in_use, true);
                } else
                    lstm::retry();
            });
            
            --p.food;
            
            std::atomic_thread_fence(std::memory_order_release);
            
            f0.in_use.unsafe_write(false);
            f1.in_use.unsafe_write(false);
        }
    };
}

int main() {
    for (std::size_t i = 0; i < repeat_count; ++i) {
        thread_manager manager;
        
        philosopher phils[5];
        fork forks[5];
        
        for (int i = 0; i < 5; ++i)
            manager.queue_thread(get_loop(phils[i], forks[i], forks[(i + 1) % 5]));
        
        manager.run();
        
        for(auto& fork : forks)
            CHECK(fork.in_use.unsafe_read() == false);
        
        for (auto& phil : phils)
            CHECK(phil.food == 0u);
        CHECK(debug_live_allocations<> == 0);
    }
    return test_result();
}
#include <lstm/lstm.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static constexpr std::size_t food_size = LSTM_TEST_INIT(10000, 100);
static constexpr std::size_t repeat_count = LSTM_TEST_INIT(300, 30);

struct LSTM_CACHE_ALIGNED philosopher {
    LSTM_CACHE_ALIGNED std::size_t food{food_size};
};

struct fork {
    var<bool, debug_alloc<bool>> in_use{false};
};

auto get_loop(philosopher& p, fork& f0, fork& f1) {
    return [&] {
        while (p.food != 0) {
            atomic([&](auto& tx) {
                if (!tx.load(f0.in_use) && !tx.load(f1.in_use)) {
                    tx.store(f0.in_use, true);
                    tx.store(f1.in_use, true);
                } else
                    lstm::retry();
            });
            
            --p.food;
            
            f0.in_use.unsafe_store(false);
            f1.in_use.unsafe_store(false);
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
            CHECK(fork.in_use.unsafe_load() == false);
        
        for (auto& phil : phils)
            CHECK(phil.food == 0u);
        CHECK(debug_live_allocations<> == 0);
    }
    return test_result();
}
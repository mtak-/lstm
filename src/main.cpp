#include <lstm/lstm.hpp>

#include "../test/thread_manager.hpp"

#include <chrono>
#include <iostream>

static constexpr auto loop_count = 100000000;

int main() {
    lstm::var<int> x{0};
    thread_manager thread_manager;
    
    for (int i = 0; i < 4; ++i) {
        thread_manager.queue_thread([&x, i] {
            int j = 0;
            while (j++ < loop_count) {
                lstm::atomic([&](auto& tx) {
                    if (j % 100 >= i*25 && j % 100 < i*25 + 5)
                        tx.store(x, tx.load(x) + 1);
                    else
                        tx.load(x);
                });
            }});
    }
    auto start = std::chrono::high_resolution_clock::now();
    thread_manager.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    lstm::atomic([&](auto& tx) {
        (void)tx;
        assert(tx.load(x) == 2000);
    });
    
    std::cout << "LSTM: "
              << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000000.f)
              << "ms"
              << std::endl;
    
    return 0;
}

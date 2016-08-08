#include <lstm/lstm.hpp>

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

LSTM_BEGIN
    std::atomic<word> transaction::clock{0};
LSTM_END

int main() {
    for(int loop = 0; loop < 100; ++loop) {
        lstm::var<int> x{0};
        int i0 = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        std::thread t0([&] {
            while (i0++ < 10000)
                lstm::atomic([&](lstm::transaction& tx) {
                    if (i0 % 100 >= 20 && i0 % 100 < 25)
                        tx.store(x, tx.load(x) + 1);
                    else
                        auto my_x = tx.load(x);
                });
        });
        
        int i1 = 0;
        std::thread t1([&] {
            while (i1++ < 10000)
                lstm::atomic([&](lstm::transaction& tx) {
                    if (i1 % 100 >= 40 && i1 % 100 < 45)
                        tx.store(x, tx.load(x) + 1);
                    else
                        auto my_x = tx.load(x);
                });
        });
        
        int i2 = 0;
        std::thread t2([&] {
            while (i2++ < 10000)
                lstm::atomic([&](lstm::transaction& tx) {
                    if (i2 % 100 >= 60 && i2 % 100 < 65)
                        tx.store(x, tx.load(x) + 1);
                    else
                        auto my_x = tx.load(x);
                });
        });
        
        int i3 = 0;
        std::thread t3([&] {
            while (i3++ < 10000)
                lstm::atomic([&](lstm::transaction& tx) {
                    if (i3 % 100 >= 0 && i3 % 100 < 5)
                        tx.store(x, tx.load(x) + 1);
                    else
                        auto my_x = tx.load(x);
                });
        });
        
        t3.join();
        t2.join();
        t1.join();
        t0.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        
        lstm::atomic([&](lstm::transaction& tx) {
            auto my_x = tx.load(x);
            assert(my_x == 2000);
        });
        
        std::cout << "LSTM: "
                  << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000000.f)
                  << "ms"
                  << std::endl;
    }

    for(int loop = 0; loop < 100; ++loop) {
        int x{0};
        std::mutex x_mut;
        int i0 = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        std::thread t0([&] {
            while (i0++ < 10000) {
                if (i0 % 100 >= 60 && i0 % 100 < 65) {
                    std::lock_guard<std::mutex> guard{x_mut};
                    ++x;
                }
                else {
                    std::lock_guard<std::mutex> guard{x_mut};
                }
            }
        });
        
        int i1 = 0;
        std::thread t1([&] {
            while (i1++ < 10000) {
                if (i1 % 100 >= 40 && i1 % 100 < 45) {
                    std::lock_guard<std::mutex> guard{x_mut};
                    ++x;
                }
                else {
                    std::lock_guard<std::mutex> guard{x_mut};
                }
            }
        });
        
        int i2 = 0;
        std::thread t2([&] {
            while (i2++ < 10000) {
                if (i2 % 100 >= 20 && i2 % 100 < 25) {
                    std::lock_guard<std::mutex> guard{x_mut};
                    ++x;
                }
                else {
                    std::lock_guard<std::mutex> guard{x_mut};
                }
            }
        });
        
        int i3 = 0;
        std::thread t3([&] {
            while (i3++ < 10000) {
                if (i3 % 100 >= 0 && i3 % 100 < 5) {
                    std::lock_guard<std::mutex> guard{x_mut};
                    ++x;
                }
                else {
                    std::lock_guard<std::mutex> guard{x_mut};
                }
            }
        });
        
        t3.join();
        t2.join();
        t1.join();
        t0.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        assert(x == 2000);
        
        std::cout << "MUT: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << "ms"
                  << std::endl;
    }
    
    return 0;
}
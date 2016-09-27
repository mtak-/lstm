#include <lstm/containers/list.hpp>

#include <cassert>
#include <iostream>
#include <thread>

void playground_entry() {
    for(int loop = 0; loop < 10; ++loop) {
        lstm::list<int> ints;

        auto start = std::chrono::high_resolution_clock::now();

        int i0 = 0;
        std::thread t0([&] {
            while (i0++ < 10000)
                ints.emplace_front(0);
        });
        
        int i1 = 0;
        std::thread t1([&] {
            while (i1++ < 10000)
                ints.clear();
        });

        int i2 = 0;
        std::thread t2([&] {
            while (i2++ < 10000)
                ints.emplace_front(2);
        });

        int i3 = 0;
        std::thread t3([&] {
            while (i3++ < 10000)
                ints.clear();
        });

        t3.join();
        t2.join();
        t1.join();
        t0.join();

        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "LIST INSERTION: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << "ms"
                  << std::endl;
    }
}
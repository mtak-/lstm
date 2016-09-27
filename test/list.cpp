#include <lstm/containers/list.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

int main() {
    // {
    //     int* i = new int(5);
    //     ++debug_live_allocations<>;
    //     debug_alloc<int> alloc;
    //     lstm::atomic([&](auto& tx) { tx.delete_(i, alloc); });
    //
    //     CHECK(debug_live_allocations<> == 0);
    // }
    // return test_result();
    static constexpr int iter_count = 10000;
    for(int loop = 0; loop < 100; ++loop) {
        {
            lstm::list<int, debug_alloc<int>> ints;
            thread_manager manager;
    
            auto start = std::chrono::high_resolution_clock::now();
    
            manager.queue_loop_n([&] { ints.emplace_front(0); }, iter_count);
            manager.queue_loop_n([&] { ints.emplace_front(0); }, iter_count);
            manager.queue_loop_n([&] { ints.clear(); }, iter_count);
            manager.queue_loop_n([&] { ints.clear(); }, iter_count);
    
            manager.run();
    
            auto end = std::chrono::high_resolution_clock::now();
    
            std::cout << "LIST INSERTION: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                      << "ms"
                      << std::endl;
    
            LSTM_LOG_DUMP();
            LSTM_LOG_CLEAR();
        }
        CHECK(debug_live_allocations<> == 0);
        if (debug_live_allocations<> != 0)
            return -1;
    }
    
    return test_result();
}
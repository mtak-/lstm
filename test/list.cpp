#include <lstm/containers/list.hpp>

#ifdef NDEBUG
#undef NDEBUG
#include "debug_alloc.hpp"
#define NDEBUG
#else
#include "debug_alloc.hpp"
#endif
#include "simple_test.hpp"
#include "thread_manager.hpp"

int main() {
    static constexpr int iter_count = 5000;
    for(int loop = 0; loop < 200; ++loop) {
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
                      
            ints.clear();
            CHECK(ints.size() == 0);
            CHECK(debug_live_allocations<> == 0);
    
            LSTM_LOG_DUMP();
            LSTM_LOG_CLEAR();
        }
    }
    
    return test_result();
}
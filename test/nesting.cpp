#include <lstm/lstm.hpp>

#include "debug_alloc.hpp"
#include "simple_test.hpp"
#include "thread_manager.hpp"

#include <cassert>
#include <thread>
#include <vector>

void push(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x, int val) {
    CHECK(lstm::in_transaction());
    
    lstm::atomic([&](lstm::transaction& tx) {
        auto var = tx.load(x);
        var.push_back(val);
        tx.store(x, std::move(var));
    });
}

void pop(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x) {
    CHECK(lstm::in_transaction());
    
    lstm::atomic([&](lstm::transaction& tx) {
        auto var = tx.load(x);
        var.pop_back();
        tx.store(x, std::move(var));
    });
}

auto get_loop(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x) {
    return [&] {
        lstm::atomic([&](auto& tx) {
            auto& var = tx.load(x);
            if (var.empty())
                push(x, 5);
            else {
                CHECK(var.size() == 1u);
                pop(x);
            }
        });
    };
}

int main() {
    {
        thread_manager manager;
        
        lstm::var<std::vector<int>, debug_alloc<std::vector<int>>> x{};
        
        for (int i = 0; i < 5; ++i)
            manager.queue_loop_n(get_loop(x), 100000);
        manager.run();
        
        CHECK(x.unsafe().empty());
    }
    CHECK(debug_live_allocations<> == 0);
    
    LSTM_LOG_DUMP();
    
    return test_result();
}
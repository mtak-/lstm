#include <lstm/lstm.hpp>

#include "simple_test.hpp"

#include <cassert>
#include <thread>
#include <vector>

void push(lstm::var<std::vector<int>>& x, int val) {
    lstm::atomic([&](auto& tx) {
        auto var = tx.load(x);
        var.push_back(val);
        tx.store(x, std::move(var));
    });
}

void pop(lstm::var<std::vector<int>>& x) {
    lstm::atomic([&](auto& tx) {
        auto var = tx.load(x);
        var.pop_back();
        tx.store(x, std::move(var));
    });
}

auto get_loop(lstm::var<std::vector<int>>& x) {
    return [&] {
        for (int i = 0; i < 100000; ++i) {
            lstm::atomic([&](auto& tx) {
                auto var = tx.load(x);
                if (var.empty())
                    push(x, 5);
                else {
                    CHECK(var.size() == 1u);
                    pop(x);
                }
            });
        }
    };
}

int main() {
    lstm::var<std::vector<int>> x{};

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i)
        threads.emplace_back(get_loop(x));
    
    for (auto& thread : threads)
        thread.join();
    CHECK(x.unsafe().empty());
    
    return test_result();
}
#include <lstm/lstm.hpp>

#include "simple_test.hpp"

#include <thread>

using lstm::atomic;
using lstm::var;

struct philosopher {
    int food{1};
};

struct fork {
    var<bool> in_use{false};
};

auto get_loop(philosopher& p, fork& f0, fork& f1) {
    return [&] {
        while (p.food != 0) {
            atomic([&](auto& tx) {
                if (!tx.load(f0.in_use) && !tx.load(f1.in_use)) {
                    tx.store(f0.in_use, true);
                    tx.store(f1.in_use, true);
                    --p.food;
                    tx.store(f0.in_use, false);
                    tx.store(f1.in_use, false);
                }
            });
        }
    };
}

int main() {
    philosopher phil, sami, eric, aimy, joey;
    fork forks[5];
    
    std::thread ts[]{
        std::thread{get_loop(phil, forks[0], forks[1])},
        std::thread{get_loop(sami, forks[1], forks[2])},
        std::thread{get_loop(eric, forks[2], forks[3])},
        std::thread{get_loop(aimy, forks[3], forks[4])},
        std::thread{get_loop(joey, forks[4], forks[0])}
    };
        
    for (auto& t : ts)
        t.join();
        
    for(auto& fork : forks)
        assert(fork.in_use.unsafe() == false);
    
    assert(phil.food == 0);
    assert(sami.food == 0);
    assert(eric.food == 0);
    assert(aimy.food == 0);
    assert(joey.food == 0);
}
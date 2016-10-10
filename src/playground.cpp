#include <lstm/easy_var.hpp>

static void foo(int&&) {}

void playground_entry() {
    lstm::easy_var<int> x;
    x = 5;
    x = 7;
    
    x = x.get() + 2;
    
    foo(x);
    
    int y;
    lstm::atomic([&] {
        x = 5;
        y = x;
    });
}
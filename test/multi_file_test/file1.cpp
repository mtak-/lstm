#include <lstm/lstm.hpp>

using namespace lstm;

struct big
{
    char bar[257];
};

extern var<int> x;
var<int>        y{0};

extern var<big> z;
var<big>        w{};

void other_function();

void run_other_file()
{
    other_function();
    atomic([](transaction tx) { y.set(tx, x.get(tx)); });
    atomic([](transaction tx) { w.set(tx, z.get(tx)); });
    atomic([](transaction tx) {
        if (x.untracked_get(tx) != 0)
            std::terminate();
    });
    atomic([](read_transaction tx) {
        if (x.get(tx) != 0)
            std::terminate();
    });
    atomic([](read_transaction tx) {
        if (x.untracked_get(tx) != 0)
            std::terminate();
    });
}

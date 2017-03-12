#include <lstm/lstm.hpp>

#include "../simple_test.hpp"

using namespace lstm;

struct big
{
    char bar[257];
};

var<int>        x{0};
extern var<int> y;

var<big>        z{};
extern var<big> w;

void run_other_file();
void other_function()
{
    atomic([](read_transaction tx) {
        if (x.get(tx) != 0)
            std::terminate();
    });
}

int main()
{
    run_other_file();
    atomic([](transaction tx) { x.set(tx, y.get(tx)); });
    atomic([](transaction tx) { z.set(tx, w.get(tx)); });
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

    return test_result();
}

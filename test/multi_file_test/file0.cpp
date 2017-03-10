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
        if (tx.read(x) != 0)
            std::terminate();
    });
}

int main()
{
    run_other_file();
    atomic([](transaction tx) { tx.write(x, tx.read(y)); });
    atomic([](transaction tx) { tx.write(z, tx.read(w)); });
    atomic([](transaction tx) {
        if (tx.untracked_read(x) != 0)
            std::terminate();
    });
    atomic([](read_transaction tx) {
        if (tx.read(x) != 0)
            std::terminate();
    });
    atomic([](read_transaction tx) {
        if (tx.untracked_read(x) != 0)
            std::terminate();
    });

    return test_result();
}

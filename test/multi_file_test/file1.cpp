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
    atomic([](transaction tx) { tx.write(y, tx.read(x)); });
    atomic([](transaction tx) { tx.write(w, tx.read(z)); });
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
}

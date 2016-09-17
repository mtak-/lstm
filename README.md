# lstm

`lstm` is an implementation of Software Transactional Memory using the Transactional Locking II model (i.e. there's actually some spinlocking in the commit phase). It fully supports nested transactions (merges them into just 1 transaction), RAII, Allocators, and exceptions (unless thrown from a destructor). It currently requires a reasonably `C++14` compliant compiler and STL, and either `__thread` or `thread_local`.

## Building Tests

Debug
```sh
$ mkdir build && cd build
$ mkdir debug && cd debug
$ cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ../..
$ make test
```

Release
```sh
$ mkdir build && cd build
$ mkdir release && cd release
$ cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
$ make test
```

## Example Code

```cpp
#include <lstm/lstm.hpp>

#include <cassert>
#include <thread>
#include <vector>

void decrement(lstm::var<int>& x) {
    lstm::atomic([&](lstm::transaction& tx) {
        tx.store(x, tx.load(x) - 1);
    });
}

void increment(lstm::var<int>& x) {
    lstm::atomic([&](lstm::transaction& tx) {
        tx.store(x, tx.load(x) + 1);
    });
}

auto get_loop(lstm::var<int>& x) {
    return [&] {
        for (int i = 0; i < 100000; ++i) {
            // auto is recommended, as calls are more likely to be inlined
            lstm::atomic([&](auto& tx) {
                if (tx.load(x) == 0)
                    increment(x);
                else
                    decrement(x);
            });
        }
    };
}

int main() {
    lstm::var<int> x{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i)
        threads.emplace_back(get_loop(x));
    
    for (auto& thread : threads)
        thread.join();
    
    assert(x.unsafe() == 0);
}
```
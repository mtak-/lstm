# lstm

`lstm` is an implementation of Software Transactional Memory using the Transactional Locking II model (i.e. there's actually some spinlocking in the commit phase). It fully supports nested transactions (merges them into just 1 transaction), RAII, Allocators, and exceptions (unless thrown from a destructor). It currently requires a reasonably `C++14` compliant compiler and STL, and `thread_local`.

## Brief description of the interface
- declare variable as shared: `lstm::var<Type> my_var(/*params*/);`
- start a transaction (or access the current transaction): `lstm::atomic([&](auto& tx) { /* code here */ });`
- load a variable from within a transaction: `const auto& my_var_val = tx.load(my_var);`
- store a variable from within a transaction: `tx.store(my_var, my_var_new_val);`
- unsafe access to a shared variable: `auto my_var_val = my_var.unsafe();`
- detect if inside of a transaction: `assert(lstm::in_transaction())`
- manually retry a transaction: `lstm::retry();`

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

## easy_var

```cpp
lstm::easy_var<int> x{100};
lstm::easy_var<int> y{0};
lstm::atomic([]{
    x -= 20;
    y += 20;
});
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
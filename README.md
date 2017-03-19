# lstm

*WIP: This library was written for fun and could have ubgs :)*

`lstm` is a header only implementation of Software Transactional Memory (STM) designed to be simple to use, and flexible. It's only been tested on Apple Clang so YMMV.

## Features

- It's header only! Simply `#include <lstm/lstm.hpp>` and you're off.
- Custom allocators are fully supported.
- C++14 implementation.
- `lstm` shares _objects_ not memory. Polymorphic, and non-POD types are safe to use.*
- Type traits are used to create user friendly `static_assert` error messages.
- If you want the library to be SFINAE friendly, simply `#define LSTM_MAKE_SFINAE_FRIENDLY`.
- The library heavily uses `<atomic>` to provide low overhead reads and writes.
- Nested transactions automatically merge into the rootmost transaction.
- Read only transactions are supported, providing a performance boost.
- Aborted transactions unwind the stack, so all of your destructors will be run.
- Lower level operations, while not the default, are exposed if you need some extra performance.
- `thread_local` means there's no need to register threads manually.
- Per thread caches result in very little overhead in maintaining a read and write set.
- Relativistic programming serves as the backbone for resource reclamation.
- The commit algorithm can be thought of as distributed `seqlock` which helps to reduce contention on cache lines.

_*_ non-POD types work as long as the following hold. 1) You don't care if an objects destructor is called later than you expect, and 2) it's ok if writing to a variable creates a new instance of that type and the old one is destroyed after the transaction completes. 1) and 2) are true for _most_ types, but not all types.

## Usage

`lstm` is header only:

1. Add `lstm/include` to your header search paths
2. In your source files `#include <lstm/lstm.hpp>`
3. All types/functions live in the `lstm::` namespace.
4. Done

## Building Tests

Debug
```sh
$ mkdir -p build/debug && cd build/debug
$ cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug ../..
$ make -j8 && make test
```

Release
```sh
$ mkdir -p build/release && cd build/release
$ cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../..
$ make -j8 && make test
```

## Getting Started

Simply wrap shared data up in a `var`, and then start a transaction as follows.

```cpp
// declare your shared variables, and initialize them as though they weren't wrapped by an lstm::var
static lstm::var<std::vector<int>> buffer{0, 1, 2, 3, 4, 5, 6};
static lstm::var<std::vector<int>> active;

std::size_t publish_buffer() {
    return lstm::atomic([&](const lstm::transaction tx) {
        std::vector<int> tmp = buffer.get(tx);
        const auto amount_published = tmp.size();
        active.set(tx, std::move(tmp));
        
        return amount_published;
    });
}
```

Transactional variables (shared variables) must be wrapped in the `var` template. `var`'s detect the constructors of the wrapped type, and have `var()` constructors to match.

A transaction is started by calling `atomic` and passing in a callable that takes, by value or `const&`, either a `transaction` (read-write) or a `read_transaction` (read only). If the callable takes both (e.g. `[](auto tx) {}`) then the `transaction` overload is preferred.

Getting values out of a `var` is as simple as calling the method `get` passing in the transaction object.

Setting values is done by calling `some_var.set(tx, value_to_set_to)`.

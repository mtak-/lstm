# lstm

`lstm` is an implementation of Software Transactional Memory designed to be simple to use, and flexible.

Techniques used include:
- The Transactional Locking II Per Object model is at the core of the commit algorithm used.
- a heavily URCU inspired algorithm is used for resource reclamation
- Up to `sizeof(std::uintptr_t) * 8 - 1` threads can reclaim shared memory concurrently (31 or 63 typically)
- `thread_local` is used to automatically track threads (no need to register them)
- nested transactions are fully supported by merging them into the root most transaction
- custom allocators are also supported
- transactions are automatically aborted and retried, when a consistent state of shared memory no longer exists
- transactions are aborted in a C++ friendly way by utilizing exceptions to unwind the stack

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

The simplest way (sacrificing some efficiency) to use `lstm` is by storing shared data in `lstm::easy_var<T>`'s. To safely access the shared data, an `lstm::atomic()` block must be created.
```cpp
bool transfer_funds(int amount, lstm::easy_var<int>& fromAccount, lstm::easy_var<int>& toAccount) {
    assert(amount > 0);
    return lstm::atomic([&]{
        if (fromAccount >= amount) {
            fromAccount -= 20;
            toAccount += 20;
            return true;
        } else {
            return false;
        }
    });
}
```

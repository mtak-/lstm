# lstm

*WIP: This library was written for fun, and is not meant for production code*

`lstm` is a header only implementation of Software Transactional Memory (STM) designed to be simple to use, and flexible. It's only been tested on Apple Clang so YMMV.

Techniques used include:
- The Transactional Locking II Per Object model was the inspiration for the commit algorithm used.
- a heavily URCU inspired algorithm is used for resource reclamation
- `thread_local` is used to automatically track threads (no need to register them)
- nested transactions are fully supported by merging them into the root most transaction
- custom allocators are also supported
- transactions are automatically aborted and retried, when a consistent state of shared memory no longer exists
- transactions are aborted in a C++ friendly way by utilizing exceptions to unwind the stack

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

Simply wrap shared data up in a `var`, and then start a `read_write` transaction as follows.

```cpp
// declare your shared variables, and initialize them as though they weren't wrapped by an lstm::var
static lstm::var<std::vector<int>> buffer{0, 1, 2, 3, 4, 5, 6};
static lstm::var<std::vector<int>> active;

std::size_t publish_buffer() {
    
    // this line either gets access to the current transaction,
    // or creates a transaction if the thread isn't currently in a critical section
    return lstm::read_write([&](lstm::transaction& tx) {
        
        // shared data can be read by calling tx.read(src)
        // if the read fails, no code below this line will be executed
        // the stack will be unwound to the point of the rootmost read_write block,
        // and the transaction will be retried
        std::vector<int> tmp = tx.read(buffer);
        const auto amount_published = tmp.size();
        
        // shared data can be written by calling tx.write(dest, data)
        // a failed write behaves the same as a failed read
        // because the stack is unwound, tmp's destructor will free any memory it holds
        tx.write(buffer, tx.read(active));
        tx.write(active, std::move(tmp));
        
        // returns the size of the shared data that was published
        return amount_published;
    }); // if this was the rootmost transaction:
            // at this point, all the writes are made visible atomically
            // if that is not possible due to another write transaction on another thread
            // the same steps as a failed read/write are taken
        // if this is not the rootmost transaction:
            // the writes are not visible to other threads until the rootmost transaction completes
            // however, the writes are always visible from within a read_write block on this thread
}
```

There's some (poorly) explained details in the comments, but basically, transactions work as you'd expect them to work. That's the point of STM.

## var

By default, `lstm` assumes data to be private. To let `lstm` know that a variable is shared, use the `var` template.

```cpp
template<typename T, typename Alloc = std::allocator<T>>
struct var;
```

### var constructors

`var`'s detect constructors in a similar fashion to `std::optional`, except `var`'s cannot be `null`, so there's no need to pass in a `std::in_place_t` into the constructor.

```cpp
var<int> x{0};
var<std::vector<int>> y{1, 2, 3, 4, 5, 6, 7, 8};
var<std::string> z{"hello world"};
var<std::string> w(3, '!');
```

Since `var` might have the need to allocate memory, an optional Allocator argument may be supplied to the constructor by using the tag `std::allocator_arg`.

```cpp
custom_alloc<int> int_alloc;
custom_alloc<std::vector<int>> vec_alloc;
custom_alloc<std::string> string_alloc;

var<int, custom_alloc<int>> x{std::allocator_arg, int_alloc, 0};
var<std::vector<int>, custom_alloc<std::vector<int>>> y{std::allocator_arg,
                                                        vec_alloc,
                                                        {1, 2, 3, 4, 5, 6, 7, 8}};
var<std::string, custom_alloc<std::string>> z{std::allocator_arg, string_alloc, "hello world"};
var<std::string, custom_alloc<std::string>> w(std::allocator_arg, string_alloc, 3, '!');
```

`var`'s are not copy/move constructible or assignable. These operations require accessing shared data, so it must be done from within a transaction context. See the section on `easy_var`.

### var member functions

```cpp
Alloc get_allocator() const noexcept;
```
`some_var.get_allocator()` returns a copy of the allocator used by `some_var`.

-

```cpp
T unsafe_read() const noexcept;
```
`some_var.unsafe_read()` returns the value contained in `var`. This function is not safe to call while transactions that might write to `some_var` are executing in any thread, or while `unsafe_write` might be executing in another thread.

-

```cpp
void unsafe_write(const T& t) noexcept;
void unsafe_write(T&& t) noexcept;
```

`some_var.unsafe_write(x)` writes the value `x` into `var`. This function is not safe to call while transactions that might read or write to `some_var` are executing in any thread; or while `unsafe_read` or `unsafe_write` might be executing in another thread.

## read_write

In order to safely read or write to shared data, a transaction context is required. Currently `read_write` is the only mechanism for obtaining a transaction context.

`read_write` is a function object of an empty and pod type providing only the callable operator.

```cpp
template<typename Func>
transact_result<Func> operator()(Func&& func,
                                 transaction_domain& domain = default_domain(),
                                 thread_data& tls_td = tls_thread_data()) const;
```

### parameters

- `Func&& func`: `func` must be callable with either no arguments, or a `transaction&` argument (preferred in the case `func` is callable under both circumstances).
- `transaction_domain& domain = default_domain()`: An lvalue reference to the global version clock to use (optional).
- `thread_data& tls_td = tls_thread_data()`: An lvalue reference to the current thread's `thread_data` instance (optional). This is provided in case accessing a `thread_local` is slow on a target. Typical uses would be to start a thread, and immediately call `thread_data& tls_td = tls_thread_data();` then perform lots of small, performance critical transactions.

### return value

`read_write` returns whatever `func` returns on transaction success.

### examples

```cpp
int x = read_write([&](transaction& tx) {
    // reading/writing of shared variables here
    return 5;
});

read_write([&] {
    // reading/writing of shared variables here
});
```

## easy_var (WIP)

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

## Performance
`TODO:`
A single threaded noop transaction which neither reads nor writes share data, has an overhead of around 6.2ns on a `2.4 GHz Intel Core i5`.

#include <lstm/lstm.hpp>

#define STATEFUL_DEBUG_ALLOC

#ifdef NDEBUG
#undef NDEBUG
#include "debug_alloc.hpp"
#define NDEBUG
#else
#include "debug_alloc.hpp"
#endif
#include "simple_test.hpp"
#include "thread_manager.hpp"

static constexpr auto loop_count = LSTM_TEST_INIT(1000000, 100000);

using lstm::atomic;
using lstm::var;

struct big
{
    char data[65];
};

int main()
{
    var<var<big, debug_alloc<big>>*> x_ptr{nullptr};
    {
        thread_manager tm;
        tm.queue_loop_n(
            [&] {
                atomic([&](const lstm::transaction tx) {
                    static std::allocator<var<big, debug_alloc<big>>> alloc{};
                    auto ptr = tx.read(x_ptr);
                    if (ptr) {
                        destroy_deallocate(alloc, ptr);
                        tx.write(x_ptr, nullptr);
                    } else {
                        tx.write(x_ptr, allocate_construct(alloc));
                    }
                });
            },
            loop_count | 1);
        tm.queue_loop_n(
            [&] {
                atomic([&](const lstm::transaction tx) {
                    auto ptr = tx.read(x_ptr);
                    if (ptr) {
                        tx.write(*ptr, big{});
                    } else
                        lstm::retry();
                });
            },
            loop_count);
        tm.run();
    }
    CHECK(debug_live_allocations<> == 1);

    return test_result();
}

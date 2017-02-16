#include <lstm/lstm.hpp>

#ifdef NDEBUG
#undef NDEBUG
#include "debug_alloc.hpp"
#define NDEBUG
#else
#include "debug_alloc.hpp"
#endif

#include "simple_test.hpp"
#include "thread_manager.hpp"

static constexpr auto loop_count = LSTM_TEST_INIT(25000, 250);

using lstm::read_write;
using lstm::var;

struct modify_on_delete
{
    int x{0};
    modify_on_delete() { throw 0; }
    ~modify_on_delete() { x = -1; }
};

struct big
{
    char data[65];
    big() noexcept = default;
    big(int) { throw 0; }
};

int main()
{
    {
        try {
            var<modify_on_delete, debug_alloc<modify_on_delete>> x;
        } catch (int) {
        }
    }
    {
        var<big, debug_alloc<big>> x;
        thread_manager tm;
        for (int i = 0; i < 5; ++i) {
            tm.queue_loop_n(
                [&] {
                    read_write([&](auto& tx) {
                        try {
                            tx.write(x, 0);
                        } catch (...) {
                            tx.write(x, {});
                        }
                    });
                },
                loop_count);
        }
        tm.run();
    }
    CHECK(debug_live_allocations<> == 0);

    return test_result();
}

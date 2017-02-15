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

static constexpr auto loop_count = 250000;

using lstm::read_write;
using lstm::var;

struct big
{
    char data[65];
    big() noexcept = default;
    big(int) { throw 0; }
};

int main()
{
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

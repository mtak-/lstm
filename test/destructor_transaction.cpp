#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static constexpr auto loop_count = LSTM_TEST_INIT(1000000, 40000);

struct vec4
{
    long long x, y, z, w;

    vec4(long long a, long long b, long long c, long long d)
        : x(a)
        , y(b)
        , z(c)
        , w(d)
    {
    }
};

static_assert(var<vec4>::atomic == false);

var<vec4> x{0, 0, 0, 0};

struct destruct
{
    ~destruct() noexcept(false)
    {
        atomic([&](const lstm::transaction tx) {
            auto y = tx.read(x);
            ++y.w;
            tx.write(x, y);
        });
    }
};

int main()
{
    {
        thread_manager tm;

        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                atomic([&](const lstm::transaction tx) {
                    auto foo = tx.read(x);
                    ++foo.x;
                    tx.write(x, foo);
                });
            }
        });

        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                atomic([&](const lstm::transaction tx) {
                    auto foo = tx.read(x);
                    ++foo.y;
                    tx.write(x, foo);
                });
            }
        });

        tm.queue_thread([&] {
            static std::allocator<destruct> alloc{};
            for (int j = 0; j < loop_count; ++j) {
                auto b = lstm::allocate_construct(alloc);
                atomic([&](const lstm::transaction tx) {
                    lstm::destroy_deallocate(alloc, b);
                    auto foo = tx.read(x);
                    ++foo.z;
                    tx.write(x, foo);
                });
            }
        });

        tm.run();
    }

    CHECK(x.unsafe_read().x == loop_count);
    CHECK(x.unsafe_read().y == loop_count);
    CHECK(x.unsafe_read().z == loop_count);
    CHECK(x.unsafe_read().w == loop_count);

    return test_result();
}

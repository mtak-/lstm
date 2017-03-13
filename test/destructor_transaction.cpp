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
            auto y = x.get(tx);
            ++y.w;
            x.set(tx, y);
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
                    auto foo = x.get(tx);
                    ++foo.x;
                    x.set(tx, foo);
                });
            }
        });

        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                atomic([&](const lstm::transaction tx) {
                    auto foo = x.get(tx);
                    ++foo.y;
                    x.set(tx, foo);
                });
            }
        });

        tm.queue_thread([&] {
            static std::allocator<destruct> alloc{};
            for (int j = 0; j < loop_count; ++j) {
                auto b = alloc.allocate(1);
                new (b) destruct();
                atomic([&](const lstm::transaction tx) {
                    lstm::destroy_deallocate(tx.get_thread_data(), alloc, b);
                    auto foo = x.get(tx);
                    ++foo.z;
                    x.set(tx, foo);
                });
            }
        });

        tm.run();
    }

    CHECK(x.unsafe_get().x == loop_count);
    CHECK(x.unsafe_get().y == loop_count);
    CHECK(x.unsafe_get().z == loop_count);
    CHECK(x.unsafe_get().w == loop_count);

    return test_result();
}

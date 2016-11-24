#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static constexpr auto loop_count = LSTM_TEST_INIT(1000000, 40000);

struct vec4 {
    long long x, y, z, w;
    
    vec4(long long a, long long b, long long c, long long d)
        : x(a), y(b), z(c), w(d) {}
};

static_assert(var<vec4>::atomic == false);

var<vec4> x{0, 0, 0, 0};

struct destruct {
    ~destruct() noexcept(false) {
        atomic([&](auto& tx) {
            auto y = tx.load(x);
            ++y.w;
            tx.store(x, y);
        });
    }
};

int main() {
    {
        thread_manager tm;
        
        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                atomic([&](auto& tx) {
                    auto foo = tx.load(x);
                    ++foo.x;
                    tx.store(x, foo);
                });
            }
        });
        
        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                atomic([&](auto& tx) {
                    auto foo = tx.load(x);
                    ++foo.y;
                    tx.store(x, foo);
                });
            }
        });
        
        tm.queue_thread([&] {
            for (int j = 0; j < loop_count; ++j) {
                auto b = new destruct();
                atomic([&](auto& tx) {
                    tx.delete_(b);
                    auto foo = tx.load(x);
                    ++foo.z;
                    tx.store(x, foo);
                });
            }
        });
        
        tm.run();
    }
    
    CHECK(x.unsafe_load().x == loop_count);
    CHECK(x.unsafe_load().y == loop_count);
    CHECK(x.unsafe_load().z == loop_count);
    
    // really >=, but if it's ==, probly worth looking at?
    CHECK(x.unsafe_load().w == loop_count);
    
    return test_result();
}

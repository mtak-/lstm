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

#include <cassert>
#include <thread>
#include <vector>

static constexpr auto loop_count = LSTM_TEST_INIT(500000, 5000);

void push(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x, int val)
{
    CHECK(lstm::tls_thread_data().in_transaction());

    lstm::atomic([&](const lstm::transaction tx) {
        auto var = x.get(tx);
        var.push_back(val);
        x.set(tx, {std::move(var)});
    });
}

void pop(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x)
{
    CHECK(lstm::tls_thread_data().in_transaction());

    lstm::atomic([&](const lstm::transaction tx) {
        auto var = x.get(tx);
        var.pop_back();
        x.set(tx, std::move(var));
    });
}

auto get_loop(lstm::var<std::vector<int>, debug_alloc<std::vector<int>>>& x)
{
    return [&] {
        lstm::atomic([&](const lstm::transaction tx) {
            auto& var = x.get(tx);
            if (var.empty())
                push(x, 5);
            else {
                CHECK(var.size() == 1u);
                pop(x);
            }
        });
    };
}

int main()
{
    {
        thread_manager manager;

        lstm::var<std::vector<int>, debug_alloc<std::vector<int>>> x{};

        for (int i = 0; i < 5; ++i)
            manager.queue_loop_n(get_loop(x), loop_count);
        manager.run();

        CHECK(x.unsafe_read().empty());
    }
    CHECK(debug_live_allocations<> == 0);

    return test_result();
}
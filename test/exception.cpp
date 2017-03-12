#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::atomic;
using lstm::var;

static constexpr auto loop_count = LSTM_TEST_INIT(200000, 4000);

int main()
{
    var<int>         x{0};
    std::atomic<int> y{0};

    {
        thread_manager tm;

        for (int i = 0; i < 5; ++i) {
            tm.queue_thread([&] {
                for (int j = 0; j < loop_count; ++j) {
                    try {
                        atomic([&](const lstm::transaction tx) {
                            int foo = x.get(tx);
                            x.set(tx, foo + 5);
                            if (foo + 5 >= 10000)
                                throw std::exception{};
                        });
                    } catch (const std::exception&) {
                        y.fetch_add(1, LSTM_RELAXED);
                    }
                }
            });
        }

        tm.run();
    }

    CHECK(x.unsafe_read() == 10000 - 5);
    CHECK(y == 5 * loop_count - 9995 / 5);

    return test_result();
}

#include <lstm/lstm.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

using lstm::thread_data;

static constexpr auto loop_count = LSTM_TEST_INIT(100000, 4000);

int main()
{
    thread_data tds[32];
    (void)tds;
    {
        thread_manager tm;

        for (int i = 0; i < 8; ++i) {
            tm.queue_thread([&] {
                for (int j = 0; j < loop_count; ++j)
                    thread_data td;
            });
        }

        tm.run();
    }

    return test_result();
}

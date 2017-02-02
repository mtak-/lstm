#include <lstm/containers/rbtree.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

#include <random>

static constexpr int iter_count   = LSTM_TEST_INIT(10000000, 1000);
static constexpr int loop_count   = LSTM_TEST_INIT(1, 1);
static constexpr int thread_count = 8;
static_assert(iter_count % thread_count == 0, "");

LSTM_NOINLINE std::vector<int> get_data()
{
    std::vector<int> data(iter_count * 2);
    std::iota(std::begin(data), std::end(data) - iter_count, 0);
    std::iota(std::begin(data) + iter_count, std::end(data), 0);

    std::mt19937 gen(42);

    std::shuffle(std::begin(data), std::end(data) - iter_count, gen);
    std::shuffle(std::begin(data) + iter_count, std::end(data), gen);

    return data;
}

int main()
{
    const auto data = get_data();
    for (int loop = 0; loop < loop_count; ++loop) {
        lstm::rbtree<int, int> intmap;
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, &data, t] {
                lstm::thread_data& tls_td = lstm::tls_thread_data();
                for (int i = 0; i < iter_count / thread_count; ++i) {
                    lstm::read_write([&data, &intmap, &tls_td, i, t](lstm::transaction& tx) {
                        intmap.emplace(tx,
                                       data[i + t * iter_count / thread_count],
                                       data[i + t * iter_count / thread_count + iter_count]);
                    });
                }
            });
        }

        auto start = std::chrono::high_resolution_clock::now();
        manager.run();
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Elapsed: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                         / 1000000.f
                  << "s" << std::endl;

        assert(lstm::read_write([&](lstm::transaction& tx) { return intmap.verify(tx); }));
    }

    return test_result();
}

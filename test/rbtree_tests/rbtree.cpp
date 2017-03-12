#include <lstm/containers/rbtree.hpp>

#include "../simple_test.hpp"
#include "../thread_manager.hpp"
#include "mallocator.hpp"

#include <random>

using tree_t = lstm::rbtree<int, int, std::less<>, mallocator<std::pair<int, int>>>;

static constexpr int elem_count   = LSTM_TEST_INIT(1000000, 1000);
static constexpr int loop_count   = LSTM_TEST_INIT(1, 1);
static constexpr int thread_count = 8;
static_assert(elem_count % thread_count == 0, "");

static LSTM_NOINLINE auto get_data()
{
    std::vector<int, mallocator<int>> data(elem_count * 2);
    std::iota(std::begin(data), std::end(data) - elem_count, 0);
    std::iota(std::begin(data) + elem_count, std::end(data), 0);

    std::mt19937 gen(42);

    std::shuffle(std::begin(data), std::end(data) - elem_count, gen);
    std::shuffle(std::begin(data) + elem_count, std::end(data), gen);

    return data;
}

static auto data = get_data();

static void populate_tree(tree_t& intmap)
{
    {
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, t] {
                lstm::thread_data& tls_td = lstm::tls_thread_data();
                for (int i = 0; i < elem_count / thread_count; ++i) {
                    lstm::atomic(tls_td, [&intmap, &tls_td, i, t](const lstm::transaction tx) {
                        intmap.emplace(tx,
                                       data[i + t * elem_count / thread_count],
                                       data[i + t * elem_count / thread_count + elem_count]);
                    });
                }
            });
        }

        auto start = std::chrono::high_resolution_clock::now();
        manager.run();
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Insert Elapsed: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                         / 1000000.f
                  << "s" << std::endl;
    }
#ifndef NDEBUG
    lstm::atomic([&](const lstm::transaction tx) { return intmap.verify(tx); });
#endif
}

static void depopulate_tree(tree_t& intmap)
{
    {
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, t] {
                lstm::thread_data& tls_td = lstm::tls_thread_data();
                for (int i = elem_count / thread_count - 1; i >= 0; --i) {
                    lstm::atomic(tls_td, [&intmap, &tls_td, i, t](const lstm::transaction tx) {
                        intmap.erase_one(tx, data[i + t * elem_count / thread_count]);
                    });
                }
            });
        }

        auto start = std::chrono::high_resolution_clock::now();
        manager.run();
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Delete Elapsed: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                         / 1000000.f
                  << "s" << std::endl;
    }
#ifndef NDEBUG
    lstm::atomic([&](const lstm::transaction tx) { return intmap.verify(tx); });
#endif
}

static void update(tree_t& intmap)
{
    {
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, t] {
                lstm::thread_data& tls_td = lstm::tls_thread_data();
                for (int i = elem_count / thread_count - 1; i >= 0; --i) {
                    // if ((i % 128) / 128.f < update_rate)
                    lstm::atomic(tls_td, [&intmap, &tls_td, i, t](const lstm::transaction tx) {
                        intmap.erase_one(tx, data[i + t * elem_count / thread_count]);
                    });
                }
            });
        }

        auto start = std::chrono::high_resolution_clock::now();
        manager.run();
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Delete Elapsed: "
                  << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
                         / 1000000.f
                  << "s" << std::endl;
    }
#ifndef NDEBUG
    lstm::atomic([&](const lstm::transaction tx) { return intmap.verify(tx); });
#endif
}

int main()
{
    for (int loop = 0; loop < loop_count; ++loop) {
        tree_t intmap;
        populate_tree(intmap);
        depopulate_tree(intmap);
    }

    return test_result();
}

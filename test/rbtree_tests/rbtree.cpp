#include <lstm/containers/rbtree.hpp>

#include "../simple_test.hpp"
#include "../thread_manager.hpp"
#include "mallocator.hpp"

#include <random>

using tree_t = lstm::rbtree<int, int, std::less<>, mallocator<std::pair<int, int>>>;

static constexpr int   elem_count   = LSTM_TEST_INIT(65536, 65536);
static constexpr int   iter_count   = LSTM_TEST_INIT(elem_count * 1000, 1000000);
static constexpr int   loop_count   = LSTM_TEST_INIT(1, 1);
static constexpr int   thread_count = 8;
static constexpr float update_rate  = 0.33f;
static_assert(elem_count % thread_count == 0, "");
static_assert(iter_count % thread_count == 0, "");
static_assert(iter_count % elem_count == 0, "");

static std::atomic<int> found_count{0};

static LSTM_NOINLINE auto get_data()
{
    std::vector<int, mallocator<int>> data;
    data.reserve(elem_count + iter_count * 2);

    std::mt19937                    gen(42);
    std::uniform_int_distribution<> dis(0, elem_count * 2 - 1);

    for (int i = 0; i < elem_count + iter_count * 2; ++i)
        data.push_back(dis(gen));

    return data;
}

static const auto data = get_data();

static void populate_tree(tree_t& intmap)
{
    {
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, t] {
                lstm::thread_data& tls_td = lstm::tls_thread_data();
                const auto         offset = t * elem_count / thread_count;
                for (int i = 0; i < elem_count / thread_count; ++i) {
                    lstm::atomic(tls_td, [&intmap, i, offset](const lstm::transaction tx) {
                        intmap.emplace(tx, data[i + offset], 42);
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

static void update(tree_t& intmap)
{
    {
        thread_manager manager;

        for (int t = 0; t < thread_count; ++t) {
            manager.queue_thread([&intmap, t] {
                int                tls_found_count = 0;
                lstm::thread_data& tls_td          = lstm::tls_thread_data();
                const auto         erase_offset    = elem_count + t * iter_count / thread_count;
                const auto emplace_offset = elem_count + iter_count + t * iter_count / thread_count;
                for (int k = 0; k < iter_count / thread_count; ++k) {
                    const float perc = (k % 128) / 128.f;
                    if (perc < update_rate) {
                        lstm::atomic(tls_td, [=, &intmap](const lstm::transaction tx) {
                            const bool result
                                = intmap.erase_one(tx, data[k + erase_offset] % elem_count);
                            (void)result;
                            LSTM_ASSERT(result);
                        });
                        lstm::atomic(tls_td, [=, &intmap](const lstm::transaction tx) {
                            intmap.emplace(tx, data[k + emplace_offset] % elem_count, 42);
                        });
                    } else {
                        const bool found
                            = lstm::atomic(tls_td, [=, &intmap](const lstm::read_transaction tx) {
                                  return intmap.find(tx, data[k + erase_offset] % elem_count);
                              });
                        if (found)
                            ++tls_found_count;
                    }
                }
                found_count += tls_found_count;
            });
        }

        auto start = std::chrono::high_resolution_clock::now();
        manager.run();
        auto elapsed = std::chrono::high_resolution_clock::now() - start;
        std::cout << "Percent Find Misses: "
                  << 1.f - found_count / (iter_count * (1.f - update_rate)) << std::endl;
        std::cout << "Update Elapsed: "
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
        update(intmap);
        lstm::atomic([&](const lstm::transaction tx) { intmap.clear(tx); });
    }

    return test_result();
}

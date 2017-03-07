#include <lstm/containers/rbtree.hpp>

#include "simple_test.hpp"
#include "thread_manager.hpp"

#include <random>

static constexpr int iter_count   = LSTM_TEST_INIT(500000, 1000);
static constexpr int loop_count   = LSTM_TEST_INIT(1, 1);
static constexpr int thread_count = 8;
static_assert(iter_count % thread_count == 0, "");

template<typename T>
struct SimpleAllocator
{
    using value_type = T;

    constexpr SimpleAllocator() noexcept = default;

    template<typename U, LSTM_REQUIRES_(!std::is_same<T, U>{})>
    LSTM_ALWAYS_INLINE constexpr SimpleAllocator(const SimpleAllocator<U>&) noexcept
    {
    }

    LSTM_ALWAYS_INLINE T* allocate(std::size_t n) const noexcept
    {
        return (T*)std::malloc(sizeof(T) * n);
    }
    constexpr LSTM_ALWAYS_INLINE void deallocate(T*, std::size_t) const noexcept {}
};

template<typename T, typename U>
bool operator==(const SimpleAllocator<T>&, const SimpleAllocator<U>&);
template<typename T, typename U>
bool operator!=(const SimpleAllocator<T>&, const SimpleAllocator<U>&);

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
    auto data = get_data();
    for (int loop = 0; loop < loop_count; ++loop) {
        lstm::rbtree<int, int, std::less<>, SimpleAllocator<std::pair<int, int>>> intmap;
        {
            thread_manager manager;

            for (int t = 0; t < thread_count; ++t) {
                if (t < thread_count / 2)
                    manager.queue_thread([&intmap, &data, t] {
                        lstm::thread_data& tls_td = lstm::tls_thread_data();
                        for (int i = 0; i < iter_count / (thread_count / 2); ++i) {
                            lstm::atomic(
                                [&data, &intmap, &tls_td, i, t](const lstm::transaction tx) {
                                    intmap.emplace(tx,
                                                   data[i + t * iter_count / (thread_count / 2)],
                                                   data[i + t * iter_count / (thread_count / 2)
                                                        + iter_count]);
                                });
                        }
                    });
                else
                    manager.queue_thread([&intmap, &data, t] {
                        lstm::thread_data& tls_td = lstm::tls_thread_data();
                        for (int i = iter_count / (thread_count / 2); i >= 0; --i) {
                            lstm::atomic([&data, &intmap, &tls_td, i, t](
                                const lstm::read_transaction tx) {
                                intmap.find(tx,
                                            data[i + (t / 2) * iter_count / (thread_count / 2)]);
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
        {
            thread_manager manager;

            for (int t = 0; t < thread_count; ++t) {
                if (t < thread_count / 2)
                    manager.queue_thread([&intmap, &data, t] {
                        lstm::thread_data& tls_td = lstm::tls_thread_data();
                        for (int i = iter_count / (thread_count / 2); i >= 0; --i) {
                            lstm::atomic([&data, &intmap, &tls_td, i, t](
                                const lstm::transaction tx) {
                                intmap.erase_one(tx, data[i + t * iter_count / (thread_count / 2)]);
                            });
                        }
                    });
                else
                    manager.queue_thread([&intmap, &data, t] {
                        lstm::thread_data& tls_td = lstm::tls_thread_data();
                        for (int i = iter_count / (thread_count / 2); i >= 0; --i) {
                            lstm::atomic([&data, &intmap, &tls_td, i, t](
                                const lstm::read_transaction tx) {
                                intmap.find(tx,
                                            data[i + (t / 2) * iter_count / (thread_count / 2)]);
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

    return test_result();
}

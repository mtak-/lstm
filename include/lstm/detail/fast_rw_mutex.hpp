#ifndef LSTM_DETAIL_FAST_RW_MUTEX_HPP
#define LSTM_DETAIL_FAST_RW_MUTEX_HPP

#include <lstm/detail/backoff.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class is probly best for use in otherwise wait-free write side algorithms
    struct fast_rw_mutex
    {
    private:
        using uint_t = uword;

        static constexpr const uint_t write_bit = uint_t(1) << (sizeof(uint_t) * 8 - 1);
        static constexpr const uint_t read_mask = ~write_bit;
        LSTM_CACHE_ALIGNED std::atomic<uint_t> read_count;

        static bool write_locked(const std::atomic<uint_t>& state) = delete;
        inline static constexpr bool write_locked(const uint_t state) noexcept
        {
            return state & write_bit;
        }

        static bool read_locked(const std::atomic<uint_t>& state) = delete;
        inline static constexpr bool read_locked(const uint_t state) noexcept
        {
            return state & read_mask;
        }

        static bool unlocked(const std::atomic<uint_t>& state) = delete;
        inline static constexpr bool unlocked(const uint_t state) noexcept { return state == 0; }

        LSTM_NOINLINE void lock_shared_slow_path() noexcept
        {
            // didn't succeed in acquiring read access, so undo the reader count increment
            read_count.fetch_sub(1, LSTM_RELAXED);

            uint_t          read_state;
            default_backoff backoff;
            do {
                backoff();
                read_state = read_count.load(LSTM_RELAXED);
            } while (write_locked(read_state)
                     || !read_count.compare_exchange_weak(read_state,
                                                          read_state + 1,
                                                          LSTM_ACQUIRE,
                                                          LSTM_RELAXED));

            assert(read_locked(read_count.load(LSTM_RELAXED)));
        }

        LSTM_NOINLINE void lock_slow_path() noexcept
        {
            uint_t prev_read_count = request_write_lock();
            wait_for_readers(prev_read_count);
        }

        uint_t request_write_lock() noexcept
        {
            uint_t          prev_read_count = read_count.load(LSTM_RELAXED);
            default_backoff backoff;
            // first come first serve
            while (write_locked(prev_read_count)
                   || !read_count.compare_exchange_weak(prev_read_count,
                                                        prev_read_count | write_bit,
                                                        LSTM_ACQUIRE,
                                                        LSTM_RELAXED)) {
                backoff();
                prev_read_count = read_count.load(LSTM_RELAXED);
            }

            return prev_read_count;
        }

        void wait_for_readers(uint_t prev_read_count) noexcept
        {
            default_backoff backoff;
            while (LSTM_LIKELY(read_locked(prev_read_count))) {
                backoff();
                prev_read_count = read_count.load(LSTM_ACQUIRE);
            }
        }

    public:
        fast_rw_mutex() noexcept
            : read_count{0}
        {
        }

#ifndef NDEBUG
        ~fast_rw_mutex() noexcept { assert(unlocked(read_count.load(LSTM_RELAXED))); }
#endif /* NDEBUG */

        LSTM_ALWAYS_INLINE void lock_shared() noexcept
        {
            if (LSTM_UNLIKELY(write_locked(read_count.fetch_add(1, LSTM_ACQUIRE))))
                lock_shared_slow_path();
        }

        LSTM_ALWAYS_INLINE void unlock_shared() noexcept
        {
            const uint_t prev = read_count.fetch_sub(1, LSTM_RELEASE);
            (void)prev;
            assert(read_locked(prev));
        }

        void lock() noexcept
        {
            uint_t zero = 0;
            if (LSTM_UNLIKELY(
                    !read_count.compare_exchange_weak(zero, write_bit, LSTM_ACQUIRE, LSTM_RELAXED)))
                lock_slow_path();
        }

        LSTM_ALWAYS_INLINE void unlock() noexcept
        {
            uint_t prev = read_count.fetch_and(read_mask, LSTM_RELEASE);
            (void)prev;
            assert(write_locked(prev));
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */

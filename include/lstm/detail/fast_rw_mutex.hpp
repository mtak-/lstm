#ifndef LSTM_DETAIL_FAST_RW_MUTEX_HPP
#define LSTM_DETAIL_FAST_RW_MUTEX_HPP

#include <lstm/detail/backoff.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class is probly best for use in otherwise wait-free write side algorithms
    struct fast_rw_mutex
    {
    private:
        // TODO: maybe should be uint32_t on x64 and might save some registers
        static constexpr const uword write_bit = uword(1) << (sizeof(uword) * 8 - 1);
        static constexpr const uword read_mask = ~write_bit;
        LSTM_CACHE_ALIGNED std::atomic<uword> read_count{0};

        static bool write_locked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool write_locked(const uword state) noexcept
        {
            return state & write_bit;
        }

        static bool read_locked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool read_locked(const uword state) noexcept
        {
            return state & read_mask;
        }

        static bool unlocked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool unlocked(const uword state) noexcept { return state == 0; }

        LSTM_NOINLINE void lock_shared_slow_path() noexcept;
        LSTM_NOINLINE void lock_slow_path() noexcept;

        uword request_write_lock() noexcept;
        void wait_for_readers(uword prev_read_count) noexcept;

    public:
        fast_rw_mutex() noexcept = default;

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
            const uword prev = read_count.fetch_sub(1, LSTM_RELEASE);
            (void)prev;
            assert(read_locked(prev));
        }

        void lock() noexcept
        {
            uword zero = 0;
            if (LSTM_UNLIKELY(
                    !read_count.compare_exchange_weak(zero, write_bit, LSTM_ACQUIRE, LSTM_RELAXED)))
                lock_slow_path();
        }

        void unlock() noexcept
        {
            uword prev = read_count.fetch_and(read_mask, LSTM_RELEASE);
            (void)prev;
            assert(write_locked(prev));
        }
    };

    LSTM_NOINLINE void fast_rw_mutex::lock_shared_slow_path() noexcept
    {
        // didn't succeed in acquiring read access, so undo the reader count increment
        read_count.fetch_sub(1, LSTM_RELAXED);

        uword           read_state;
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

    LSTM_NOINLINE void fast_rw_mutex::lock_slow_path() noexcept
    {
        uword prev_read_count = request_write_lock();
        wait_for_readers(prev_read_count);
    }

    uword fast_rw_mutex::request_write_lock() noexcept
    {
        uword           prev_read_count = read_count.load(LSTM_RELAXED);
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

    void fast_rw_mutex::wait_for_readers(uword prev_read_count) noexcept
    {
        default_backoff backoff;
        while (LSTM_LIKELY(read_locked(prev_read_count))) {
            backoff();
            prev_read_count = read_count.load(LSTM_ACQUIRE);
        }
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */

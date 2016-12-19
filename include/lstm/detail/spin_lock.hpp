#ifndef LSTM_DETAIL_SPIN_LOCK_HPP
#define LSTM_DETAIL_SPIN_LOCK_HPP

#include <lstm/detail/backoff.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct LSTM_CACHE_ALIGNED spin_lock {
    private:
        LSTM_CACHE_ALIGNED std::atomic<uint8_t> flag{0};
        
        template<typename Backoff>
        LSTM_NOINLINE void lock_slow_path(Backoff& backoff) noexcept {
            flag.fetch_sub(1, LSTM_RELAXED);
            
            uint8_t zero;
            
            do {
                zero = 0;
                backoff();
            } while (!flag.compare_exchange_weak(zero, 1, LSTM_RELAXED, LSTM_RELAXED));
            
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
    public:
        spin_lock() noexcept = default;
    #ifndef NDEBUG
        ~spin_lock() noexcept { assert(!flag.load(LSTM_RELAXED)); }
    #endif
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        LSTM_ALWAYS_INLINE void lock(Backoff backoff = {}) noexcept {
            if (LSTM_UNLIKELY(flag.fetch_add(1, LSTM_ACQUIRE)))
                lock_slow_path(backoff);
        }
        
        LSTM_ALWAYS_INLINE void unlock() noexcept {
            uword prev = flag.fetch_sub(1, LSTM_RELEASE);
            (void)prev;
            assert(prev);
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_SPIN_LOCK_HPP */

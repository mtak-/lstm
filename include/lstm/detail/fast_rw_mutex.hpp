#ifndef LSTM_DETAIL_FAST_RW_MUTEX_HPP
#define LSTM_DETAIL_FAST_RW_MUTEX_HPP

#include <lstm/detail/backoff.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class is probly best for use in otherwise wait-free write side algorithms
    struct LSTM_CACHE_ALIGNED fast_rw_mutex {
    private:
        // TODO: maybe should be uint32_t on x64 and might save some registers
        static constexpr const uword write_bit = uword(1) << (sizeof(uword) * 8 - 1);
        static constexpr const uword read_mask = ~write_bit;
        LSTM_CACHE_ALIGNED std::atomic<uword> read_count{0};
        
        static bool write_locked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool write_locked(const uword state) noexcept
        { return state & write_bit; }
        
        static bool read_locked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool read_locked(const uword state) noexcept
        { return state & read_mask; }
        
        static bool unlocked(const std::atomic<uword>& state) = delete;
        inline static constexpr bool unlocked(const uword state) noexcept { return state == 0; }
        
        template<typename Backoff>
        LSTM_NOINLINE void lock_shared_slow_path(Backoff& backoff) noexcept;
        
        template<typename Backoff>
        uword request_write_lock(Backoff backoff);
        
        template<typename Backoff>
        void wait_for_readers(Backoff backoff, uword prev_read_count);
        
    public:
        fast_rw_mutex() noexcept = default;
        
    #ifndef NDEBUG
        ~fast_rw_mutex() noexcept { assert(unlocked(read_count.load(LSTM_RELAXED))); }
    #endif /* NDEBUG */
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        inline void lock_shared(Backoff backoff = {}) noexcept {
            if (LSTM_UNLIKELY(write_locked(read_count.fetch_add(1, LSTM_ACQUIRE))))
                lock_shared_slow_path(backoff);
        }
        
        inline void unlock_shared() noexcept {
            uword prev = read_count.fetch_sub(1, LSTM_RELEASE);
            (void)prev;
            assert(read_locked(prev));
        }
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        inline void lock(Backoff backoff = {}) noexcept {
            uword prev_read_count = request_write_lock(backoff);
            wait_for_readers(backoff, prev_read_count);
            
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void unlock() noexcept {
            uword prev = read_count.fetch_and(read_mask, LSTM_RELEASE);
            (void)prev;
            assert(write_locked(prev));
        }
    };
    
    template<typename Backoff>
    LSTM_NOINLINE void fast_rw_mutex::lock_shared_slow_path(Backoff& backoff) noexcept {
        // didn't succeed in acquiring read access, so undo the reader count increment
        uword read_state = read_count.fetch_sub(1, LSTM_RELAXED);
        
        do {
            backoff();
            read_state &= read_mask;
            // TODO: is CAS the best way to do this? it doesn't get run very often...
            while (!write_locked(read_state)
                && !read_count.compare_exchange_weak(read_state,
                                                     read_state + 1,
                                                     LSTM_RELAXED,
                                                     LSTM_RELAXED));
        } while (write_locked(read_state));
        
        std::atomic_thread_fence(LSTM_ACQUIRE);
        
        assert(read_locked(read_count.load(LSTM_RELAXED)));
    }
    
    template<typename Backoff>
    uword fast_rw_mutex::request_write_lock(Backoff backoff) {
        uword prev_read_count = read_count.fetch_or(write_bit, LSTM_RELAXED);
        
        // first come first serve
        while (write_locked(prev_read_count)) {
            backoff();
            prev_read_count = read_count.fetch_or(write_bit, LSTM_RELAXED);
        }
        
        return prev_read_count;
    }
    
    template<typename Backoff>
    void fast_rw_mutex::wait_for_readers(Backoff backoff, uword prev_read_count) {
        while (read_locked(prev_read_count)) {
            backoff();
            prev_read_count = read_count.load(LSTM_RELAXED);
        }
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */

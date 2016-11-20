#ifndef LSTM_DETAIL_FAST_RW_MUTEX_HPP
#define LSTM_DETAIL_FAST_RW_MUTEX_HPP

#include <lstm/detail/backoff.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class is probly best for use in otherwise wait-free write side algorithms
    struct fast_rw_mutex {
    private:
        static constexpr uword write_bit = uword(1) << (sizeof(uword) * 8 - 1);
        std::atomic<uword> read_count{0};
        
        inline static constexpr bool write_locked(uword state) noexcept
        { return state & write_bit; }
        
        inline static constexpr bool read_locked(uword state) noexcept
        { return state & ~write_bit; }
        
        inline static bool write_locked(const std::atomic<uword>& state) noexcept = delete;
        inline static bool read_locked(const std::atomic<uword>& state) noexcept = delete;
        
        template<typename Backoff>
        void lock_shared_slow_path(Backoff& backoff) noexcept {
            // didn't succeed in acquiring read access, so undo, the reader count increment
            read_count.fetch_sub(1, LSTM_RELAXED);
            
            uword read_state;
            do {
                backoff();
                read_state = 0;
                // TODO: is CAS the best way to do this? it doesn't get run very often...
                while (!write_locked(read_state)
                    && !read_count.compare_exchange_weak(read_state,
                                                         read_state + 1,
                                                         LSTM_RELAXED,
                                                         LSTM_RELAXED));
            } while (write_locked(read_state));
            
            assert(!write_locked(read_state));
            assert(read_locked(read_count.load(LSTM_RELAXED)));
        }
        
    public:
        fast_rw_mutex() noexcept = default;
        
    #ifndef NDEBUG
        ~fast_rw_mutex() noexcept { assert(read_count.load(LSTM_ACQUIRE) == 0); }
    #endif /* NDEBUG */
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        inline void lock_shared(Backoff backoff = {}) noexcept {
            // if there's a writer, do the slow stuff
            if (write_locked(read_count.fetch_add(1, LSTM_ACQUIRE)))
                lock_shared_slow_path(backoff);
        }
        
        void unlock_shared() noexcept {
            uword prev = read_count.fetch_sub(1, LSTM_RELEASE);
            (void)prev;
            assert(read_locked(prev));
        }
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        void lock(Backoff backoff = {}) noexcept {
            // get the acquire out of the way as well...
            // tsan was complaining about atomic-fence synchronization?
            uword prev_read_count = prev_read_count = read_count.fetch_or(write_bit, LSTM_ACQUIRE);
            
            // first signal that we want to write, while checking if another thread is
            // already in line to write. first come first serve
            while (write_locked(prev_read_count)) {
                backoff();
                prev_read_count = read_count.fetch_or(write_bit, LSTM_RELAXED);
            }
            
            backoff.reset();
            
            assert(!write_locked(prev_read_count));
            
            // now, wait for all readers to finish
            while (read_locked(prev_read_count)) {
                backoff();
                prev_read_count = read_count.load(LSTM_RELAXED);
            }
            
            // we have the lock
            assert(!read_locked(prev_read_count));
        }
        
        void unlock() noexcept {
            uword prev = read_count.fetch_and(~write_bit, LSTM_RELEASE);
            (void)prev;
            assert(write_locked(prev));
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */

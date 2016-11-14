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
        
        template<typename Backoff>
        void lock_shared_slow_path(Backoff backoff) noexcept {
            // didn't succeed in acquiring read access, so undo, the reader count increment
            read_count.fetch_sub(1, LSTM_RELAXED);
            
            uword read_state;
            do {
                backoff();
                read_state = 0;
                // TODO: is CAS the best way to do this? it doesn't get run very often...
                while (!(read_state & write_bit)
                    && !read_count.compare_exchange_weak(read_state,
                                                         read_state + 1,
                                                         LSTM_RELAXED,
                                                         LSTM_RELAXED));
                
            } while (read_state & write_bit);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        
    public:
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        inline void lock_shared(Backoff backoff = {}) noexcept {
            // if there's a writer, do the slow stuff
            if (read_count.fetch_add(1, LSTM_ACQUIRE) & write_bit)
                lock_shared_slow_path(std::move(backoff));
        }
        
        void unlock_shared() noexcept { read_count.fetch_sub(1, LSTM_RELEASE); }
        
        template<typename Backoff = default_backoff,
            LSTM_REQUIRES_(is_backoff_strategy<Backoff>())>
        void lock(Backoff backoff = {}) noexcept {
            uword prev_read_count;
            
            // first signal that we want to write, whilst checking if another thread is
            // already in line to write. first come first serve
            while ((prev_read_count = read_count.fetch_or(write_bit, LSTM_RELAXED)) & write_bit)
                backoff();
            
            backoff.reset();
            
            // now, wait for all readers to finish
            while (prev_read_count & ~write_bit) {
                backoff();
                prev_read_count = read_count.load(LSTM_RELAXED);
            }
            
            // we have the lock!
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        void unlock() noexcept {
            uword prev = read_count.fetch_and(~write_bit, LSTM_RELEASE);
            (void)prev;
            assert(prev == write_bit);
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */

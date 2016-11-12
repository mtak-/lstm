#ifndef LSTM_DETAIL_FAST_RW_MUTEX_HPP
#define LSTM_DETAIL_FAST_RW_MUTEX_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    // this class is probly best for use in wait-free write side algorithms
    struct fast_rw_mutex {
    private:
        static constexpr word write_bit = word(1) << (sizeof(word) * 8 - 1);
        std::atomic<word> read_count{0};
        
    public:
        // TODO: optimal memory ordering
        void lock_shared() noexcept {
            word read_state;
            do {
                read_state = 0;
                while (!(read_state & write_bit)
                    && !read_count.compare_exchange_weak(read_state,
                                                         read_state + 1,
                                                         LSTM_RELAXED,
                                                         LSTM_RELAXED));
                
            } while (read_state & write_bit);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        void unlock_shared() noexcept { read_count.fetch_sub(1, LSTM_RELEASE); }
        
        void lock() noexcept {
            word zero = 0;
            while (!read_count.compare_exchange_weak(zero, write_bit))
                zero = 0;
        }
        void unlock() noexcept {
            read_count.store(0, LSTM_RELEASE);
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_FAST_RW_MUTEX_HPP */
#ifndef LSTM_DETAIL_QUIESCENCE_HPP
#define LSTM_DETAIL_QUIESCENCE_HPP

#include <lstm/detail/lstm_fwd.hpp>
#include <lstm/detail/thread_local.hpp>

#include <mutex>

LSTM_DETAIL_BEGIN
    struct quiescence;

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::mutex quiescence_mut{};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<quiescence*> quiescence_root{nullptr};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<std::int_fast8_t> grace_period{1};

    struct quiescence {
        std::atomic<std::int_fast8_t> active;
        quiescence* next;
        
        // TODO: wait/lock-free version of constructor/destructor
        // then noexcept will be correct
        quiescence() noexcept
            : active(0)
        {
            std::lock_guard<std::mutex> guard{quiescence_mut<>};
            next = quiescence_root<>.load(LSTM_RELAXED);
            quiescence_root<>.store(this, LSTM_RELAXED);
        }
        
        // TODO: wait/lock-free version of constructor/destructor
        // then noexcept will be correct
        ~quiescence() noexcept {
            std::lock_guard<std::mutex> guard{quiescence_mut<>};
            quiescence* root_ptr = quiescence_root<>.load(LSTM_RELAXED);
            assert(root_ptr != nullptr);
            if (root_ptr == this)
                quiescence_root<>.store(next, LSTM_RELAXED);
            else {
                assert(root_ptr != nullptr);
                while (root_ptr != nullptr && root_ptr->next != this) root_ptr = root_ptr->next;
                assert(root_ptr != nullptr);
                root_ptr->next = next;
            }
        }
        
        quiescence(const quiescence&) = delete;
        quiescence& operator=(const quiescence&) = delete;
    };
    
    inline quiescence& tls_quiescence() noexcept {
        static LSTM_THREAD_LOCAL quiescence data;
        return data;
    }
    
    inline void access_lock() noexcept {
       quiescence& q = tls_quiescence();
       q.active.store(grace_period<>.load(LSTM_RELAXED), LSTM_RELAXED);
       std::atomic_thread_fence(LSTM_ACQUIRE);
    }
    
    inline void access_unlock() noexcept
    { tls_quiescence().active.store(0, LSTM_RELEASE); }
    
    inline bool check_grace_period(quiescence& q) noexcept {
        std::int_fast8_t const v = q.active.load(LSTM_RELAXED);
        return v && (v ^ grace_period<>.load(LSTM_RELAXED));
    }
    
    inline void flip_and_wait() noexcept {
        grace_period<>.fetch_xor(1, LSTM_SEQ_CST);
        for (quiescence* q = quiescence_root<>.load(LSTM_ACQUIRE);
                q != nullptr;
                q = q->next) {
            while (check_grace_period(*q)) {
                __asm__ __volatile__ ("" ::: "memory");
            }
        }
    }
    
    inline void synchronize() noexcept {
       std::atomic_thread_fence(LSTM_ACQUIRE);
       {
          std::lock_guard<std::mutex> guard(quiescence_mut<>);
          flip_and_wait();
          flip_and_wait();
       }
       std::atomic_thread_fence(LSTM_ACQUIRE);
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_HPP */

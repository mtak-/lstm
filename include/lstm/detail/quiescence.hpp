#ifndef LSTM_DETAIL_QUIESCENCE_HPP
#define LSTM_DETAIL_QUIESCENCE_HPP

#include <lstm/detail/fast_rw_mutex.hpp>
#include <lstm/detail/thread_local.hpp>

#include <algorithm>

LSTM_DETAIL_BEGIN
    struct quiescence;

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    fast_rw_mutex quiescence_mut{};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<quiescence*> quiescence_root{nullptr};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<std::uint_fast8_t> grace_period{1};

    struct quiescence {
        std::atomic<std::uint_fast8_t> active;
        quiescence* next;
        
        // TODO: make actually noexcept
        quiescence() noexcept
            : active(0)
        {
            quiescence_mut<>.lock();
            next = quiescence_root<>.load(LSTM_RELAXED);
            quiescence_root<>.store(this, LSTM_RELAXED);
            quiescence_mut<>.unlock();
        }
        
        // TODO: make actually noexcept
        ~quiescence() noexcept {
            quiescence_mut<>.lock();
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
            quiescence_mut<>.unlock();
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
    
    inline bool check_grace_period(const quiescence& q, const std::uint_fast8_t gp) noexcept {
        std::uint_fast8_t const v = q.active.load(LSTM_RELAXED);
        return v && (v ^ gp);
    }
    
    inline void wait(const std::uint_fast8_t gp) noexcept {
        for (quiescence* q = quiescence_root<>.load(LSTM_ACQUIRE);
                q != nullptr;
                q = q->next) {
            while (check_grace_period(*q, gp)) {
                __asm__ __volatile__ ("" ::: "memory");
            }
        }
    }
    
    inline void flip(std::uint_fast8_t& gp, std::uint_fast8_t& gp2) noexcept {
        do {
            gp2 = ~gp & -~gp;
        } while (!gp2 || !grace_period<>.compare_exchange_weak(gp,
                                                               gp ^ gp2,
                                                               LSTM_RELAXED,
                                                               LSTM_RELEASE));
    }
    
    inline void synchronize() noexcept {
        std::atomic_thread_fence(LSTM_ACQUIRE);
        {
            std::uint_fast8_t gp2;
            std::uint_fast8_t gp = grace_period<>.load(LSTM_RELAXED);
            flip(gp, gp2);
            
            quiescence_mut<>.lock_shared();
            wait(gp2);
            grace_period<>.fetch_xor(gp2, LSTM_RELEASE);
            wait(gp2);
            quiescence_mut<>.unlock_shared();
        }
        std::atomic_thread_fence(LSTM_ACQUIRE);
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_HPP */

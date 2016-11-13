#ifndef LSTM_DETAIL_QUIESCENCE_HPP
#define LSTM_DETAIL_QUIESCENCE_HPP

#include <lstm/detail/exponential_delay.hpp>
#include <lstm/detail/fast_rw_mutex.hpp>
#include <lstm/detail/thread_local.hpp>

LSTM_DETAIL_BEGIN
    struct quiescence;
    using gp_t = std::uint_fast8_t;

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    fast_rw_mutex quiescence_mut{};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<quiescence*> quiescence_root{nullptr};

    // TODO: inline variable
    template<std::nullptr_t = nullptr>
    std::atomic<gp_t> grace_period{1};
    
    // with the guarantee of no nested critical sections only one bit is needed
    // to say a thread is active.
    // this means the remaining bits can be used for the grace period, resulting
    // in concurrent writes
    // still not ideal, as writes should be batched
    struct quiescence {
        std::atomic<gp_t> active;
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
       tls_quiescence().active.store(grace_period<>.load(LSTM_RELAXED), LSTM_RELAXED);
       std::atomic_thread_fence(LSTM_ACQUIRE);
    }
    
    inline void access_unlock() noexcept
    { tls_quiescence().active.store(0, LSTM_RELEASE); }
    
    inline bool check_grace_period(const quiescence& q,
                                   const gp_t gp,
                                   const bool desired) noexcept {
        const gp_t thread_gp = q.active.load(LSTM_RELAXED);
        return thread_gp && !!(thread_gp & gp) == desired;
    }
    
    inline void wait(const gp_t gp, const bool desired) noexcept {
        for (quiescence* q = quiescence_root<>.load(LSTM_ACQUIRE);
                q != nullptr;
                q = q->next) {
            exponential_delay<100000, 10000000> exp_delay;
            while (check_grace_period(*q, gp, desired)) {
                exp_delay();
            }
        }
    }
    
    // TODO: kill the CAS operation? (speculative xor, might actually decrease grace period times)
    // TODO: kill the thread fences?
    inline void synchronize() noexcept {
        std::atomic_thread_fence(LSTM_ACQUIRE);
        {
            gp_t gp2 = 0;
            gp_t gp;
            
            do {
                if (!gp2) gp = grace_period<>.load(LSTM_RELAXED);
                gp2 = ~gp & -~gp;
            } while (!gp2 || !grace_period<>.compare_exchange_weak(gp,
                                                                   gp ^ gp2,
                                                                   LSTM_RELAXED,
                                                                   LSTM_RELEASE));
            
            quiescence_mut<>.lock_shared();
            wait(gp2, false);
            grace_period<>.fetch_xor(gp2, LSTM_RELEASE);
            wait(gp2, true);
            quiescence_mut<>.unlock_shared();
        }
        std::atomic_thread_fence(LSTM_ACQUIRE);
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_HPP */

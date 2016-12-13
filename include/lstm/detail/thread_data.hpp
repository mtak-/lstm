#ifndef LSTM_DETAIL_THREAD_DATA_HPP
#define LSTM_DETAIL_THREAD_DATA_HPP

#include <lstm/detail/fast_rw_mutex.hpp>

LSTM_DETAIL_BEGIN
    struct thread_data;
    using gp_t = uword;

    LSTM_INLINE_VAR(fast_rw_mutex thread_data_mut){}; // already cache aligned
    LSTM_INLINE_VAR(LSTM_CACHE_ALIGNED std::atomic<thread_data*> thread_data_root){nullptr};
    LSTM_INLINE_VAR(LSTM_CACHE_ALIGNED std::atomic<gp_t> grace_period){1};
    
    // with the guarantee of no nested critical sections only one bit is needed
    // to say a thread is active.
    // this means the remaining bits can be used for the grace period, resulting
    // in concurrent writes
    // still not ideal, as writes should be batched
    struct LSTM_CACHE_ALIGNED thread_data {
        transaction* tx;
        LSTM_CACHE_ALIGNED std::atomic<gp_t> active;
        LSTM_CACHE_ALIGNED std::atomic<thread_data*> next;
        
        thread_data() noexcept
            : tx(nullptr)
        {
            active.store(0, LSTM_RELEASE); // this must happen before locking
                                           // otherwise deadlock could occur if
                                           // a thread is synchronizing as it will read
                                           // garbage otherwise
            
            LSTM_ACCESS_INLINE_VAR(thread_data_mut).lock();
                
                next.store(LSTM_ACCESS_INLINE_VAR(thread_data_root).load(LSTM_RELAXED),
                           LSTM_RELAXED);
                LSTM_ACCESS_INLINE_VAR(thread_data_root).store(this, LSTM_RELAXED);
                
            LSTM_ACCESS_INLINE_VAR(thread_data_mut).unlock();
        }
        
        ~thread_data() noexcept {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == 0);
            
            LSTM_ACCESS_INLINE_VAR(thread_data_mut).lock();
                
                std::atomic<thread_data*>* indirect = &LSTM_ACCESS_INLINE_VAR(thread_data_root);
                while (indirect->load(LSTM_RELAXED) != this)
                    indirect = &indirect->load(LSTM_RELAXED)->next;
                
                indirect->store(next.load(LSTM_RELAXED), LSTM_RELAXED);
                
            LSTM_ACCESS_INLINE_VAR(thread_data_mut).unlock();
        }
        
        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;
        
        inline void access_lock() noexcept {
            assert(!active.load(LSTM_RELAXED));
            active.store(LSTM_ACCESS_INLINE_VAR(grace_period).load(LSTM_RELAXED), LSTM_RELAXED);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void access_unlock() noexcept {
            assert(!!active.load(LSTM_RELAXED));
            active.store(0, LSTM_RELEASE);
        }
    };
    
    LSTM_INLINE_VAR(LSTM_THREAD_LOCAL thread_data _tls_thread_data){};
    
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept {
        auto* result = &LSTM_ACCESS_INLINE_VAR(_tls_thread_data);
        LSTM_ESCAPE_VAR(result); // atleast on darwin, this helps significantly
        return *result;
    }
    
    inline bool not_in_grace_period(const thread_data& q,
                                    const gp_t gp,
                                    const bool desired) noexcept {
        // TODO: acquire seems unneeded
        const gp_t thread_gp = q.active.load(LSTM_ACQUIRE);
        return thread_gp && !!(thread_gp & gp) == desired;
    }
    
    // TODO: allow specifying a backoff strategy
    inline void wait(const gp_t gp, const bool desired) noexcept {
        for (thread_data* q = LSTM_ACCESS_INLINE_VAR(thread_data_root).load(LSTM_RELAXED);
                q != nullptr;
                q = q->next.load(LSTM_RELAXED)) {
            default_backoff backoff;
            while (not_in_grace_period(*q, gp, desired))
                backoff();
        }
    }
    
    // TODO: kill the CAS operation? (speculative or, might actually decrease grace period times)
    // TODO: write a better less confusing implementation
    inline gp_t acquire_gp_bit() noexcept {
        gp_t gp2 = 0;
        gp_t gp;
        
        do {
            if (!gp2) gp = LSTM_ACCESS_INLINE_VAR(grace_period).load(LSTM_RELAXED);
            gp2 = ~gp & -~gp;
        } while (!gp2 || !LSTM_ACCESS_INLINE_VAR(grace_period)
                            .compare_exchange_weak(gp,
                                                   gp ^ gp2,
                                                   LSTM_RELAXED,
                                                   LSTM_RELEASE));
        return gp2;
    }
    
    // TODO: allow specifying a backoff strategy
    inline void synchronize() noexcept {
        assert(!tls_thread_data().active.load(LSTM_RELAXED));
        
        gp_t gp = acquire_gp_bit();
        
        LSTM_ACCESS_INLINE_VAR(thread_data_mut).lock_shared();
        
            wait(gp, false);
            
            // TODO: release seems unneeded
            LSTM_ACCESS_INLINE_VAR(grace_period).fetch_xor(gp, LSTM_RELEASE);
            
            wait(gp, true);
        
        LSTM_ACCESS_INLINE_VAR(thread_data_mut).unlock_shared();
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_THREAD_DATA_HPP */

#ifndef LSTM_DETAIL_THREAD_DATA_HPP
#define LSTM_DETAIL_THREAD_DATA_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/atomic_swap_pod_vector.hpp>

#include <mutex>

LSTM_DETAIL_BEGIN
    struct thread_data;
    using gp_t = uword;
    
    using mutex_type = std::mutex;
    
    struct global_data {
        mutex_type thread_data_mut{};
        thread_data* thread_data_root{nullptr};
        LSTM_CACHE_ALIGNED std::atomic<gp_t> grace_period{1};
    };
    
    LSTM_INLINE_VAR LSTM_CACHE_ALIGNED global_data globals{};
    
    inline void lock_all_thread_data() noexcept;
    inline void unlock_all_thread_data() noexcept;
    inline void do_all_callbacks(thread_data& tls_td);
    
    // with the guarantee of no nested critical sections only one bit is needed
    // to say a thread is active.
    // this means the remaining bits can be used for the grace period, resulting
    // in concurrent writes
    struct LSTM_CACHE_ALIGNED thread_data {
        transaction* tx;
        
        // TODO: this is a terrible type for gp_callbacks
        // need some type with an atomic swap
        small_pod_vector<gp_callback_buf, 128, std::allocator<gp_callback_buf>> gp_callbacks;
        
        LSTM_CACHE_ALIGNED mutex_type mut;
        LSTM_CACHE_ALIGNED std::atomic<gp_t> active;
        LSTM_CACHE_ALIGNED thread_data* next;
        
        LSTM_NOINLINE thread_data() noexcept
            : tx(nullptr)
        {
            active.store(0, LSTM_RELEASE);
            
            mut.lock();
            
            lock_all_thread_data();
                
                next = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
                LSTM_ACCESS_INLINE_VAR(globals).thread_data_root = this;
                
            unlock_all_thread_data();
        }
        
        LSTM_NOINLINE ~thread_data() noexcept {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == 0);
            
            do_callbacks();
            gp_callbacks.reset();
            
            lock_all_thread_data();
            
                global_data& globals_ = LSTM_ACCESS_INLINE_VAR(globals);
                thread_data** indirect = &globals_.thread_data_root;
                while (*indirect != this)
                    indirect = &(*indirect)->next;
                *indirect = next;
                
                mut.unlock();
                
            unlock_all_thread_data();
        }
        
        thread_data(const thread_data&) = delete;
        thread_data& operator=(const thread_data&) = delete;
        
        // TODO: when atomic swap on gp_callbacks is possible, this needs to do just that
        void do_callbacks() noexcept {
            for (auto& gp_callback : gp_callbacks)
                gp_callback();
            gp_callbacks.clear();
        }
        
        inline void access_lock() noexcept {
            assert(!active.load(LSTM_RELAXED));
            active.store(LSTM_ACCESS_INLINE_VAR(globals).grace_period.load(LSTM_RELAXED),
                         LSTM_RELAXED);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void access_unlock() noexcept {
            assert(!!active.load(LSTM_RELAXED));
            active.store(0, LSTM_RELEASE);
        }
    };
    
    // TODO: still feel like this garbage is overkill
    LSTM_INLINE_VAR LSTM_THREAD_LOCAL thread_data _tls_thread_data{};
    
    LSTM_ALWAYS_INLINE thread_data& tls_thread_data() noexcept {
        auto* result = &LSTM_ACCESS_INLINE_VAR(_tls_thread_data);
        LSTM_ESCAPE_VAR(result); // atleast on darwin, this helps significantly
        return *result;
    }
    
    inline void lock_all_thread_data() noexcept {
        LSTM_ACCESS_INLINE_VAR(globals).thread_data_mut.lock();
        
        thread_data* current = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
        while (current) {
            current->mut.lock();
            current = current->next;
        }
    }
    
    inline void unlock_all_thread_data() noexcept {
        thread_data* current = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
        while (current) {
            current->mut.unlock();
            current = current->next;
        }
        
        LSTM_ACCESS_INLINE_VAR(globals).thread_data_mut.unlock();
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
        for (thread_data* q = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
                q != nullptr;
                q = q->next) {
            default_backoff backoff;
            while (not_in_grace_period(*q, gp, desired))
                backoff();
        }
    }
    
    // TODO: kill the CAS operation?
    // TODO: write a better less confusing implementation
    inline gp_t acquire_gp_bit() noexcept {
        std::atomic<gp_t>& grace_period_ref = LSTM_ACCESS_INLINE_VAR(globals).grace_period;
        gp_t gp = grace_period_ref.load(LSTM_RELAXED);
        gp_t gp_bit;
        default_backoff backoff{};
        
        do {
            while (LSTM_UNLIKELY(gp == std::numeric_limits<gp_t>::max())) {
                backoff();
                gp = grace_period_ref.load(LSTM_RELAXED);
            }
            
            gp_bit = ~gp & -~gp;
        } while (LSTM_UNLIKELY(!grace_period_ref.compare_exchange_weak(gp,
                                                                       gp | gp_bit,
                                                                       LSTM_RELAXED,
                                                                       LSTM_RELAXED)));
        return gp_bit;
    }
    
    // TODO: allow specifying a backoff strategy
    inline void synchronize(mutex_type& mut) noexcept {
        assert(!tls_thread_data().active.load(LSTM_RELAXED));
        
        gp_t gp = acquire_gp_bit();
        
        mut.lock();
        
            wait(gp, false);
            
            // TODO: release seems unneeded
            LSTM_ACCESS_INLINE_VAR(globals).grace_period.fetch_xor(gp, LSTM_RELEASE);
            
            wait(gp, true);
        
        mut.unlock();
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_THREAD_DATA_HPP */

#ifndef LSTM_DETAIL_THREAD_DATA_HPP
#define LSTM_DETAIL_THREAD_DATA_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/small_pod_vector.hpp>

#include <mutex>

LSTM_DETAIL_BEGIN
    struct thread_data;
    using gp_t = uword;
    static constexpr gp_t off_state = ~gp_t(0);
    
    using mutex_type = std::mutex;
    
    struct global_data {
        mutex_type thread_data_mut{};
        thread_data* thread_data_root{nullptr};
        LSTM_CACHE_ALIGNED std::atomic<gp_t> grace_period{0};
    };
    
    LSTM_INLINE_VAR LSTM_CACHE_ALIGNED global_data globals{};
    
    inline void lock_all_thread_data() noexcept;
    inline void unlock_all_thread_data() noexcept;
    
    // with the guarantee of no nested critical sections only one bit is needed
    // to say a thread is active.
    // this means the remaining bits can be used for the grace period, resulting
    // in concurrent writes
    struct LSTM_CACHE_ALIGNED thread_data {
        transaction* tx;
        
        // TODO: this is a terrible type for gp_callbacks
        // also probly should be a few different kinds of callbacks (succ/fail/always)
        // sharing of gp_callbacks would be nice, but how could it be made fast for small tx's?
        small_pod_vector<gp_callback_buf, 1> gp_callbacks;
        
        LSTM_CACHE_ALIGNED mutex_type mut;
        LSTM_CACHE_ALIGNED std::atomic<gp_t> active;
        LSTM_CACHE_ALIGNED thread_data* next;
        
        LSTM_NOINLINE thread_data() noexcept
            : tx(nullptr)
        {
            active.store(off_state, LSTM_RELEASE);
            
            mut.lock();
            
            lock_all_thread_data();
                
                next = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
                LSTM_ACCESS_INLINE_VAR(globals).thread_data_root = this;
                
            unlock_all_thread_data();
        }
        
        LSTM_NOINLINE ~thread_data() noexcept {
            assert(tx == nullptr);
            assert(active.load(LSTM_RELAXED) == off_state);
            
            lock_all_thread_data();
                
                thread_data** indirect = &LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
                while (*indirect != this)
                    indirect = &(*indirect)->next;
                *indirect = next;
                
                mut.unlock();
                
            unlock_all_thread_data();
            
            gp_callbacks.reset();
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
            assert(active.load(LSTM_RELAXED) == off_state);
            active.store(LSTM_ACCESS_INLINE_VAR(globals).grace_period.load(LSTM_RELAXED),
                         LSTM_RELAXED);
            std::atomic_thread_fence(LSTM_ACQUIRE);
        }
        
        inline void access_unlock() noexcept {
            assert(active.load(LSTM_RELAXED) != off_state);
            active.store(off_state, LSTM_RELEASE);
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
    
    inline bool not_in_grace_period(const thread_data& q, const gp_t gp) noexcept {
        // TODO: acquire seems unneeded
        const gp_t thread_gp = q.active.load(LSTM_ACQUIRE);
        return thread_gp <= gp;
    }
    
    // TODO: allow specifying a backoff strategy
    inline void wait(const gp_t gp) noexcept {
        for (thread_data* q = LSTM_ACCESS_INLINE_VAR(globals).thread_data_root;
                q != nullptr;
                q = q->next) {
            default_backoff backoff;
            while (not_in_grace_period(*q, gp))
                backoff();
        }
    }
    
    // TODO: allow specifying a backoff strategy
    inline void synchronize(mutex_type& mut) noexcept {
        assert(tls_thread_data().active.load(LSTM_RELAXED) == off_state);
        
        gp_t gp = LSTM_ACCESS_INLINE_VAR(globals).grace_period.fetch_add(1, LSTM_RELEASE);
        assert(gp != off_state);
        
        mut.lock();
        
            wait(gp);
        
        mut.unlock();
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_THREAD_DATA_HPP */

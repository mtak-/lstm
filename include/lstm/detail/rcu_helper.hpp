#ifndef LSTM_DETAIL_RCU_HELPER_HPP
#define LSTM_DETAIL_RCU_HELPER_HPP

#include <urcu.h>

LSTM_DETAIL_BEGIN
    static inline bool startup_rcu() noexcept {
        rcu_init();
        return true;
    }
    
    struct rcu_thread_registerer {
        rcu_thread_registerer() noexcept { rcu_register_thread(); }
        rcu_thread_registerer(const rcu_thread_registerer&) = delete;
        rcu_thread_registerer& operator=(const rcu_thread_registerer&) = delete;
        ~rcu_thread_registerer() noexcept { rcu_unregister_thread(); }
    };
    
    struct rcu_lock_guard {
    private:
        bool engaged = true;
        
    public:
        inline rcu_lock_guard() noexcept { rcu_read_lock(); }
        rcu_lock_guard(const rcu_lock_guard&) = delete;
        rcu_lock_guard& operator=(const rcu_lock_guard&) = delete;
        inline ~rcu_lock_guard() noexcept { if (engaged) rcu_read_unlock(); }
        
        void release() noexcept { engaged = false; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RCU_HELPER_HPP */
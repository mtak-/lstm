#ifndef LSTM_DETAIL_RCU_HELPER_HPP
#define LSTM_DETAIL_RCU_HELPER_HPP

#include <lstm/detail/thread_data.hpp>

LSTM_DETAIL_BEGIN
    struct rcu_lock_guard {
    private:
        thread_data& tls_td;
        bool engaged = true;
        
    public:
        inline rcu_lock_guard(thread_data& in_tls_td) noexcept
            : tls_td(in_tls_td)
        { tls_td.access_lock(); }
        
        inline ~rcu_lock_guard() noexcept { if (engaged) tls_td.access_unlock(); }
        
        rcu_lock_guard(const rcu_lock_guard&) = delete;
        rcu_lock_guard& operator=(const rcu_lock_guard&) = delete;
        
        void release() noexcept { engaged = false; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RCU_HELPER_HPP */
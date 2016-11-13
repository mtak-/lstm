#ifndef LSTM_DETAIL_RCU_HELPER_HPP
#define LSTM_DETAIL_RCU_HELPER_HPP

#include <lstm/detail/quiescence.hpp>

LSTM_DETAIL_BEGIN
    struct rcu_lock_guard {
    private:
        quiescence& tls_q;
        bool engaged = true;
        
    public:
        inline rcu_lock_guard(quiescence& in_tls_q) noexcept
            : tls_q(in_tls_q)
        { tls_q.access_lock(); }
        
        inline ~rcu_lock_guard() noexcept { if (engaged) tls_q.access_unlock(); }
        
        rcu_lock_guard(const rcu_lock_guard&) = delete;
        rcu_lock_guard& operator=(const rcu_lock_guard&) = delete;
        
        void release() noexcept { engaged = false; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RCU_HELPER_HPP */
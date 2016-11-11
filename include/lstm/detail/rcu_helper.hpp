#ifndef LSTM_DETAIL_RCU_HELPER_HPP
#define LSTM_DETAIL_RCU_HELPER_HPP

#include <lstm/detail/quiescence.hpp>

LSTM_DETAIL_BEGIN
    struct rcu_lock_guard {
    private:
        bool engaged = true;
        
    public:
        inline rcu_lock_guard() noexcept { access_lock(); }
        rcu_lock_guard(const rcu_lock_guard&) = delete;
        rcu_lock_guard& operator=(const rcu_lock_guard&) = delete;
        inline ~rcu_lock_guard() noexcept { if (engaged) access_unlock(); }
        
        void release() noexcept { engaged = false; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RCU_HELPER_HPP */
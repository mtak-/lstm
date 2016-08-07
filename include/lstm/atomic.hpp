#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

LSTM_BEGIN
    template<typename Func>
    void atomic(const Func& func) {
        while(true) {
            try {
                transaction tx{};
                ((Func&&)func)(tx);
                tx.commit();
                break;
            } catch(const tx_retry&) {}
        }
    }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
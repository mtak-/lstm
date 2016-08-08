#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    template<typename...>
    using void_ = void;

    template<typename Func, typename = void>
    struct is_void_transact_function : std::false_type {};
    
    template<typename Func>
    struct is_void_transact_function<
        Func,
        std::enable_if_t<
            std::is_void<decltype(std::declval<const Func&>()(std::declval<transaction&>()))>{}>>
        : std::true_type {};
        
    template<typename Func, typename = void>
    struct is_transact_function : std::false_type {};
    
    template<typename Func>
    struct is_transact_function<
        Func,
        void_<decltype(std::declval<const Func&>()(std::declval<transaction&>()))>>
        : std::true_type {};

    struct atomic_fn {
        // TODO: noexcept
        template<typename Func,
            LSTM_REQUIRES_(is_transact_function<Func>() && !is_void_transact_function<Func>())>
        decltype(auto) operator()(Func func) const {
            while(true) {
                try {
                    transaction tx{};
                    decltype(auto) result = func(tx);
                    tx.commit();
                    return result;
                } catch(const tx_retry&) {}
            }
        }
        
        // TODO: noexcept
        template<typename Func,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        void operator()(Func func) const {
            while(true) {
                try {
                    transaction tx{};
                    func(tx);
                    tx.commit();
                    break;
                } catch(const tx_retry&) {}
            }
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Func>
    decltype(auto) atomic(Func func) { return detail::atomic_fn{}(std::move(func)); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
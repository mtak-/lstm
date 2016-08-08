#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    template<typename...>
    using void_ = void;

    template<typename Func, typename Alloc, typename = void>
    struct is_void_transact_function : std::false_type {};
    
    template<typename Func, typename Alloc>
    struct is_void_transact_function<
        Func,
        Alloc,
        std::enable_if_t<
            std::is_void<decltype(std::declval<const Func&>()(std::declval<transaction<Alloc>&>()))>{}>>
        : std::true_type {};
        
    template<typename Func, typename Alloc, typename = void>
    struct is_transact_function : std::false_type {};
    
    template<typename Func, typename Alloc>
    struct is_transact_function<
        Func,
        Alloc,
        void_<decltype(std::declval<const Func&>()(std::declval<transaction<Alloc>&>()))>>
        : std::true_type {};

    struct atomic_fn {
        // TODO: noexcept
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            LSTM_REQUIRES_(is_transact_function<Func, Alloc>()
                && !is_void_transact_function<Func, Alloc>())>
        decltype(auto) operator()(Func func, Alloc alloc = Alloc{}) const {
            while(true) {
                try {
                    transaction<Alloc> tx{alloc};
                    decltype(auto) result = func(tx);
                    tx.commit();
                    return result;
                } catch(const tx_retry&) {}
            }
        }
        
        // TODO: noexcept
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            LSTM_REQUIRES_(is_void_transact_function<Func, Alloc>())>
        void operator()(Func func, Alloc alloc = Alloc{}) const {
            while(true) {
                try {
                    transaction<Alloc> tx{alloc};
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
#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

#include <lstm/detail/thread_local.hpp>

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
    private:
        // TODO: Alloc... maybe the rootmost transaction does have control over allocation, which
        // means transaction needs some kinda base class, with type erasure, or virtual methods :(
        template<typename Alloc>
        static transaction<Alloc>*& thread_local_transaction() {
            static LSTM_THREAD_LOCAL transaction<Alloc>* tx = nullptr;
            return tx;
        }
        
    public:
        // TODO: noexcept
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            LSTM_REQUIRES_(is_transact_function<Func, Alloc>()
                && !is_void_transact_function<Func, Alloc>())>
        decltype(auto) operator()(Func func, const Alloc& alloc = Alloc{}) const {
            static constexpr auto tx_size = sizeof(transaction<Alloc>);
            alignas(transaction<Alloc>) char storage[tx_size];
            auto& tx = thread_local_transaction<Alloc>();
            bool owns_tx = !tx;
            
            while(true) {
                try {
                    if (owns_tx) {
                        new(storage) transaction<Alloc>{alloc};
                        tx = reinterpret_cast<transaction<Alloc>*>(storage);
                    }
                    
                    decltype(auto) result = func(*tx);
                    
                    if (owns_tx) {
                        tx->commit();
                        tx->~transaction();
                        tx = nullptr;
                    }
                    return result;
                } catch(const tx_retry&) {
                    if (owns_tx)
                        tx->~transaction();
                    else
                        throw;
                }
            }
        }
        
        // TODO: noexcept
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            LSTM_REQUIRES_(is_void_transact_function<Func, Alloc>())>
        void operator()(Func func, const Alloc& alloc = Alloc{}) const {
            static constexpr auto tx_size = sizeof(transaction<Alloc>);
            alignas(transaction<Alloc>) char storage[tx_size];
            auto& tx = thread_local_transaction<Alloc>();
            bool owns_tx = !tx;
            
            while(true) {
                try {
                    if (owns_tx) {
                        new(storage) transaction<Alloc>{alloc};
                        tx = reinterpret_cast<transaction<Alloc>*>(storage);
                    }
                    
                    func(*tx);
                    
                    if (owns_tx) {
                        tx->commit();
                        tx->~transaction();
                        tx = nullptr;
                    }
                    break;
                } catch(const tx_retry&) {
                    if (owns_tx)
                        tx->~transaction();
                    else
                        throw;
                }
            }
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Func>
    decltype(auto) atomic(Func func) { return detail::atomic_fn{}(std::move(func)); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
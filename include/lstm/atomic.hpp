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
        
    template<typename Func, typename Alloc>
    using result_type = decltype(std::declval<Func>()(std::declval<transaction<Alloc>&>()));

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
        template<typename Func, typename Alloc,
            LSTM_REQUIRES_(is_transact_function<Func, Alloc>()
                && !is_void_transact_function<Func, Alloc>())>
        result_type<Func, Alloc> operator()(Func func, const Alloc& alloc) const {
            static constexpr auto tx_size = sizeof(transaction<Alloc>);
            alignas(transaction<Alloc>) char storage[tx_size];
            auto& tls_tx = thread_local_transaction<Alloc>();
            
            // this thread is already in a transaction
            if (tls_tx) {
                auto stack_tx_ptr = tls_tx;
                return func(*stack_tx_ptr);
            }
            
            auto stack_tx_ptr = reinterpret_cast<transaction<Alloc>*>(storage);
            tls_tx = stack_tx_ptr;
            
            while(true) {
                try {
                    new(storage) transaction<Alloc>{alloc};
                    
                    decltype(auto) result = func(*stack_tx_ptr);
                    
                    stack_tx_ptr->commit();
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->~transaction();
                    tls_tx = nullptr;
                        
                    return result;
                } catch(const tx_retry&) {
                    try {
                        stack_tx_ptr->cleanup();
                    } catch(...) {
                        tls_tx = nullptr;
                        stack_tx_ptr->~transaction();
                        throw;
                    }
                    stack_tx_ptr->~transaction();
                } catch(...) {
                    tls_tx = nullptr;
                    
                    try {
                        stack_tx_ptr->cleanup();
                    } catch(...) {
                        std::terminate(); // two exceptions at once, no way to recover
                    }
                    stack_tx_ptr->~transaction();
                    throw;
                }
            }
        }
        
        // TODO: noexcept
        template<typename Func, typename Alloc,
            LSTM_REQUIRES_(is_void_transact_function<Func, Alloc>())>
        void operator()(Func func, const Alloc& alloc) const {
            static constexpr auto tx_size = sizeof(transaction<Alloc>);
            alignas(transaction<Alloc>) char storage[tx_size];
            auto& tls_tx = thread_local_transaction<Alloc>();
            
            // this thread is already in a transaction
            if (tls_tx) {
                auto stack_tx_ptr = tls_tx;
                return func(*stack_tx_ptr);
            }
            
            auto stack_tx_ptr = reinterpret_cast<transaction<Alloc>*>(storage);
            tls_tx = stack_tx_ptr;
            
            while(true) {
                try {
                    new(storage) transaction<Alloc>{alloc};
                    
                    func(*stack_tx_ptr);
                    
                    stack_tx_ptr->commit();
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->~transaction();
                    tls_tx = nullptr;
                        
                    break;
                } catch(const tx_retry&) {
                    try {
                        stack_tx_ptr->cleanup();
                    } catch(...) {
                        tls_tx = nullptr;
                        stack_tx_ptr->~transaction();
                        throw;
                    }
                    stack_tx_ptr->~transaction();
                } catch(...) {
                    tls_tx = nullptr;
                    
                    try {
                        stack_tx_ptr->cleanup();
                    } catch(...) {
                        std::terminate(); // two exceptions at once, no way to recover
                    }
                    stack_tx_ptr->~transaction();
                    throw;
                }
            }
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Func, typename Alloc = std::allocator<detail::var_base*>>
    detail::result_type<Func, std::allocator<detail::var_base*>>
    atomic(Func func, const Alloc& alloc = {})
    { return detail::atomic_fn{}(std::move(func), alloc); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
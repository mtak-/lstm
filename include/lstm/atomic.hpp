#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

#include <lstm/detail/thread_local.hpp>

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
        
    template<typename Func>
    using result_type = decltype(std::declval<Func>()(std::declval<transaction&>()));

    struct atomic_fn {
    private:
        // TODO: Alloc: see transaction.hpp
#ifdef LSTM_THREAD_LOCAL
        static inline ::lstm::transaction*& tls_transaction() noexcept {
            static LSTM_THREAD_LOCAL ::lstm::transaction* tx = nullptr;
            return tx;
        }
#else
    #error "TODO: pthreads implementation of thread locals"
        static inline ::lstm::transaction*& tls_transaction() noexcept {
            // TODO: this
        }
#endif
        
    public:
        template<typename Func, typename Alloc,
            LSTM_REQUIRES_(is_transact_function<Func>()
                && !is_void_transact_function<Func>())>
        result_type<Func> operator()(Func func, const Alloc& alloc) const {
            auto& tls_tx = tls_transaction();
            // this thread is already in a transaction
            if (tls_tx) {
                auto stack_tx_ptr = tls_tx;
                return func(*stack_tx_ptr);
            }
            
            static constexpr auto tx_size = sizeof(transaction_impl<Alloc>);
            alignas(transaction_impl<Alloc>) char storage[tx_size];
            
            auto stack_tx_ptr = reinterpret_cast<transaction_impl<Alloc>*>(storage);
            tls_tx = stack_tx_ptr;
            
            new(storage) transaction_impl<Alloc>{alloc};
            while(true) {
                try {
                    assert(stack_tx_ptr->read_set.size() == 0);
                    assert(stack_tx_ptr->write_set.size() == 0);
                    
                    decltype(auto) result = func(*stack_tx_ptr);
                    
                    stack_tx_ptr->commit();
                    stack_tx_ptr->~transaction_impl();
                    tls_tx = nullptr;
                        
                    return result;
                } catch(const tx_retry&) {
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->read_version = transaction::get_clock();
                } catch(...) {
                    tls_tx = nullptr;
                    
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->~transaction_impl();
                    throw;
                }
            }
        }
        
        template<typename Func, typename Alloc,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        void operator()(Func func, const Alloc& alloc) const {
            auto& tls_tx = tls_transaction();
            if (tls_tx) {
                // this thread is already in a transaction
                auto stack_tx_ptr = tls_tx;
                return func(*stack_tx_ptr);
            }
            
            static constexpr auto tx_size = sizeof(transaction_impl<Alloc>);
            alignas(transaction_impl<Alloc>) char storage[tx_size];
            
            auto stack_tx_ptr = reinterpret_cast<transaction_impl<Alloc>*>(storage);
            tls_tx = stack_tx_ptr;
            
            new(storage) transaction_impl<Alloc>{alloc};
            while(true) {
                try {
                    assert(stack_tx_ptr->read_set.size() == 0);
                    assert(stack_tx_ptr->write_set.size() == 0);
                    
                    func(*stack_tx_ptr);
                    
                    stack_tx_ptr->commit();
                    stack_tx_ptr->~transaction_impl();
                    tls_tx = nullptr;
                        
                    break;
                } catch(const tx_retry&) {
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->read_version = transaction::get_clock();
                } catch(...) {
                    tls_tx = nullptr;
                    
                    stack_tx_ptr->cleanup();
                    stack_tx_ptr->~transaction_impl();
                    throw;
                }
            }
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Func, typename Alloc = std::allocator<detail::var_base*>>
    detail::result_type<Func> atomic(Func func, const Alloc& alloc = {})
    { return detail::atomic_fn{}(std::move(func), alloc); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
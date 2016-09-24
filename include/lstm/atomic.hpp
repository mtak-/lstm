#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

#include <lstm/detail/scope_guard.hpp>
#include <lstm/detail/thread_local.hpp>

LSTM_DETAIL_BEGIN
    template<typename...>
    using void_ = void;

    template<typename Func, typename = void>
    struct is_void_transact_function : std::false_type {};
    
    template<typename Func>
    using transact_result = decltype(std::declval<const Func&>()(std::declval<transaction&>()));
    
    template<typename Func>
    struct is_void_transact_function<Func, std::enable_if_t<std::is_void<transact_result<Func>>{}>>
        : std::true_type {};
        
    template<typename Func, typename = void>
    struct is_transact_function : std::false_type {};
    
    template<typename Func>
    struct is_transact_function<Func, void_<transact_result<Func>>> : std::true_type {};
    
    template<typename T>
    using uninitialized = std::aligned_storage_t<sizeof(T), alignof(T)>;

    struct atomic_fn {
    private:
#ifdef LSTM_THREAD_LOCAL
        static inline transaction*& tls_transaction() noexcept {
            static LSTM_THREAD_LOCAL transaction* tx = nullptr;
            return tx;
        }
#else
    #error "TODO: pthreads implementation of thread locals"
        static inline transaction*& tls_transaction() noexcept { /* TODO: this */ }
#endif /* LSTM_THREAD_LOCAL */
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        void try_transact(Func& func, Tx& tx) const {
            func(tx);
            tx.commit();
        }
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        transact_result<Func> try_transact(Func& func, Tx& tx) const {
            decltype(auto) result = func(tx);
            tx.commit();
            return static_cast<transact_result<Func>>(result);
        }
        
    public:
        bool in_transaction() const noexcept { return tls_transaction() != nullptr; }
        
        template<typename Func, typename Alloc>
        transact_result<Func> operator()(Func func, const Alloc& alloc) const {
            auto& tls_tx = tls_transaction();
            // this thread is already in a transaction
            if (tls_tx)
                return func(*tls_tx);
            
            transaction_impl<Alloc> tx{alloc};
            tls_tx = &tx;
            
            const auto disable_active_transaction = make_scope_guard([&] { tls_tx = nullptr; });
            while(true) {
                try {
                    assert(tx.read_set.size() == 0);
                    assert(tx.write_set.size() == 0);
                    
                    return try_transact(func, tx);
                } catch(const _tx_retry&) {
                    tx.cleanup();
                    tx.read_version = transaction::get_clock();
                } catch(...) {
                    tx.cleanup();
                    throw;
                }
            }
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
        LSTM_REQUIRES_(detail::is_transact_function<Func>())>
    detail::transact_result<Func> atomic(Func func, const Alloc& alloc = {})
    { return detail::atomic_fn{}(std::move(func), alloc); }
    
    inline bool in_transaction() noexcept { return detail::atomic_fn{}.in_transaction(); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
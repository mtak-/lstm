#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/transaction.hpp>

#include <lstm/detail/scope_guard.hpp>
#include <lstm/detail/thread_local.hpp>

#include <lstm/detail/rcu_helper.hpp>

LSTM_DETAIL_BEGIN
    struct atomic_fn {
    private:
#ifdef LSTM_THREAD_LOCAL
        static inline transaction*& tls_transaction() noexcept {
            static bool rcu_initialized = startup_rcu();
            static LSTM_THREAD_LOCAL rcu_thread_registerer registerer{};
            static LSTM_THREAD_LOCAL transaction* tx = nullptr;
            return tx;
        }
#else
    #error "TODO: pthreads implementation of thread locals"
        static inline transaction*& tls_transaction() noexcept { /* TODO: this */ }
#endif /* LSTM_THREAD_LOCAL */

        static inline void reset_active_transaction() noexcept { tls_transaction() = nullptr; }
        
        template<typename Func, typename Tx, typename ScopeGuard,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        static void try_transact(Func& func, Tx& tx, ScopeGuard scope_guard) {
            {
                rcu_lock_guard rcu_guard;
                func(tx);
            }
            tx.commit();
            scope_guard.release();
        }
        
        template<typename Func, typename Tx, typename ScopeGuard,
            LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        static transact_result<Func> try_transact(Func& func, Tx& tx, ScopeGuard scope_guard) {
            rcu_lock_guard rcu_guard;
            decltype(auto) result = func(tx);
            rcu_guard.release();
            rcu_read_unlock();
            
            tx.commit();
            scope_guard.release();
            return static_cast<transact_result<Func>&&>(result);
        }
        
        template<typename Func, typename Alloc>
        static transact_result<Func> atomic_slow_path(Func func,
                                                      transaction_domain* domain,
                                                      const Alloc& alloc) {
            transaction_impl<Alloc> tx{domain, alloc};
            tls_transaction() = &tx;
            
            const auto tls_guard = make_scope_guard([]() noexcept
                                                    { atomic_fn::reset_active_transaction(); });
            while(true) {
                try {
                    assert(tx.read_set.size() == 0);
                    assert(tx.write_set.size() == 0);
                    
                    return atomic_fn::try_transact(func, tx, make_scope_guard([&]() noexcept
                                                                              { tx.cleanup(); }));
                } catch(const _tx_retry&) { tx.reset_read_version(); }
            }
        }
        
    public:
        inline bool in_transaction() const noexcept { return tls_transaction() != nullptr; }
        
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            LSTM_REQUIRES_(detail::is_transact_function<Func>())>
        transact_result<Func> operator()(Func func,
                                         transaction_domain* domain = nullptr,
                                         const Alloc& alloc = {}) const {
            auto tls_tx = tls_transaction();
            if (tls_tx) return std::move(func)(*tls_tx); // TODO: which domain should this use???
            
            return atomic_fn::atomic_slow_path(std::move(func), domain, alloc);
        }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename T> struct static_const { static constexpr const T value{}; };
    template<typename T> constexpr T static_const<T>::value;
    namespace { constexpr auto&& atomic = static_const<detail::atomic_fn>::value; }
    
    inline bool in_transaction() noexcept { return detail::atomic_fn{}.in_transaction(); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/detail/thread_data.hpp>

#include <lstm/transaction.hpp>

LSTM_BEGIN
    // optional knobs
    template<std::size_t MaxStackReadBuffSize = 4,
             std::size_t MaxStackWriteBuffSize = 4,
             std::size_t MaxStackDeleterBuffSize = 4>
    struct knobs {};
LSTM_END

LSTM_DETAIL_BEGIN
    struct atomic_fn {
    private:
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(callable_with_tx<Func, Tx&>())>
        static decltype(auto) call(Func& func, Tx& tx) {
            static_assert(!noexcept(func(tx)),
                "functions passed to atomic must not be marked noexcept");
            return func(tx);
        }
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(!callable_with_tx<Func, Tx&>())>
        static decltype(auto) call(Func& func, Tx&) {
            static_assert(!noexcept(func()),
                "functions passed to atomic must not be marked noexcept");
            return func();
        }
        
        // heavy use of exceptions for lack of a better control flow
        // means RAII has a bit of a penalty
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        static void try_transact(Func& func, Tx& tx, thread_data& tls_td) {
            {
                tls_td.access_lock();
                call(func, tx);
                tls_td.access_unlock();
            }
            
            // TODO: release the tls transaction, this allows destructors of deleted objects
            // to have a transaction
            tx.commit();
            tls_td.tx = nullptr;
        }
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        static transact_result<Func>
        try_transact(Func& func, Tx& tx, thread_data& tls_td) {
            tls_td.access_lock();
            decltype(auto) result = call(func, tx);
            tls_td.access_unlock();
            
            // TODO: release the tls transaction, this allows destructors of deleted objects
            // to have a transaction
            tx.commit();
            tls_td.tx = nullptr;
            return static_cast<transact_result<Func>&&>(result);
        }
        
        template<typename Func, typename Alloc, std::size_t ReadSize, std::size_t WriteSize,
            std::size_t DeleteSize>
        static transact_result<Func> atomic_slow_path(Func func,
                                                      transaction_domain* domain,
                                                      const Alloc& alloc,
                                                      knobs<ReadSize, WriteSize, DeleteSize>,
                                                      thread_data& tls_td) {
            transaction_impl<Alloc, ReadSize, WriteSize, DeleteSize> tx{domain, alloc};
            tls_td.tx = &tx;
            
            while(true) {
                try {
                    assert(tx.read_set.size() == 0);
                    assert(tx.write_set.size() == 0);
                    
                    return atomic_fn::try_transact(func, tx, tls_td);
                } catch(const _tx_retry&) {
                    tls_td.access_unlock();
                    tx.cleanup();
                    tx.reset_read_version();
                } catch(...) {
                    tls_td.access_unlock();
                    tx.cleanup();
                    tls_td.tx = nullptr;
                    throw;
                }
            }
        }
        
    public:
        inline bool in_transaction() const noexcept
        { return tls_thread_data().tx != nullptr; }
        
        template<typename Func, typename Alloc = std::allocator<detail::var_base*>,
            std::size_t MaxStackReadBuffSize = 4,
            std::size_t MaxStackWriteBuffSize = 4,
            std::size_t MaxStackDeleterBuffSize = 4,
            LSTM_REQUIRES_(detail::is_transact_function<Func>())>
        transact_result<Func> operator()(Func func,
                                         transaction_domain* domain = nullptr,
                                         const Alloc& alloc = {},
                                         knobs<MaxStackWriteBuffSize,
                                               MaxStackReadBuffSize,
                                               MaxStackDeleterBuffSize> knobs = {}) const {
            auto& tls_td = tls_thread_data();
            
            if (tls_td.tx)
                return call(func, *tls_td.tx);
            
            return atomic_fn::atomic_slow_path(std::move(func), domain, alloc, knobs, tls_td);
        }
    };

    template<typename T> constexpr const T static_const{};
LSTM_DETAIL_END

LSTM_BEGIN
    namespace { constexpr auto&& atomic = detail::static_const<detail::atomic_fn>; }
    
    inline bool in_transaction() noexcept { return detail::atomic_fn{}.in_transaction(); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */

#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/detail/thread_data.hpp>
#include <lstm/detail/tx_result_buffer.hpp>

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
        // means RAII has a bit of a penalty in some circumstances
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(is_void_transact_function<Func>())>
        static bool try_transact(Func& func, Tx& tx, thread_data& tls_td, tx_result_buffer<void>&) {
            tls_td.access_lock();
            call(func, tx);
            tls_td.tx = nullptr; // in the future, unlocking, might trigger a transaction
            tls_td.access_unlock();
            
            return tx.commit();
        }
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        static bool try_transact(Func& func, Tx& tx,
                                 thread_data& tls_td,
                                 tx_result_buffer<transact_result<Func>>& buf) {
            tls_td.access_lock();
            buf.emplace(call(func, tx));
            tls_td.tx = nullptr; // in the future, unlocking, might trigger a transaction
            tls_td.access_unlock();
            
            return tx.commit();
        }
        
        template<typename Func, typename Alloc, std::size_t ReadSize, std::size_t WriteSize,
            std::size_t DeleteSize>
        static transact_result<Func> atomic_slow_path(Func func,
                                                      transaction_domain* domain,
                                                      const Alloc& alloc,
                                                      knobs<ReadSize, WriteSize, DeleteSize>,
                                                      thread_data& tls_td) {
            tx_result_buffer<transact_result<Func>> buf;
            transaction_impl<Alloc, ReadSize, WriteSize, DeleteSize> tx{domain, alloc};
            tls_td.tx = &tx;
            
            while(true) {
                try {
                    assert(tx.read_set.size() == 0);
                    assert(tx.write_set.size() == 0);
                    
                    if (atomic_fn::try_transact(func, tx, tls_td, buf)) {
                        tx.reset_heap();
                        return return_tx_result_buffer_fn{}(buf);
                    }
                    buf.reset();
                    tx.cleanup();
                    tls_td.tx = &tx;
                    tx.reset_read_version();
                } catch(const _tx_retry&) {
                    tls_td.access_unlock();
                    tx.cleanup();
                    tx.reset_read_version();
                } catch(...) {
                    tls_td.access_unlock();
                    tx.cleanup();
                    tx.reset_heap();
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
        
#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func, typename... Args,
            LSTM_REQUIRES_(!detail::is_transact_function<Func>())>
        void operator()(Func, Args&&...) const {
            static_assert(detail::is_transact_function<Func>(),
                "functions passed to lstm::atomic must either take no parameters, "
                "lstm::transaction&, or auto&/T&");
        }
#endif
    };

    template<typename T> constexpr const T static_const{};
LSTM_DETAIL_END

LSTM_BEGIN
    namespace { constexpr auto&& atomic = detail::static_const<detail::atomic_fn>; }
    
    inline bool in_transaction() noexcept { return detail::atomic_fn{}.in_transaction(); }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */

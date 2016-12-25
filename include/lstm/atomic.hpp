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
        static auto call(Func& func, Tx& tx) -> decltype(func(tx)) {
            static_assert(!noexcept(func(tx)),
                "functions passed to atomic must not be marked noexcept");
            return func(tx);
        }
        
        template<typename Func, typename Tx,
            LSTM_REQUIRES_(!callable_with_tx<Func, Tx&>())>
        static auto call(Func& func, Tx&) -> decltype(func()) {
            static_assert(!noexcept(func()),
                "functions passed to atomic must not be marked noexcept");
            return func();
        }
        
        template<typename Func, typename Tx>
        static void try_transact(Func& func, Tx& tx, tx_result_buffer<void>&)
        { call(func, tx); }
        
        template<typename Func, typename Tx>
        static void try_transact(Func& func, Tx& tx, tx_result_buffer<transact_result<Func>>& buf)
        { buf.emplace(call(func, tx)); }
        
        template<typename Tx>
        [[noreturn]] static void unhandled_exception(Tx& tx, thread_data& tls_td) {
            tx.cleanup();
            tls_td.access_unlock();
            tls_td.tx = nullptr;
            tx.reset_heap();
            throw;
        }
        
        template<typename Tx>
        LSTM_ALWAYS_INLINE static void tx_internal_failed(Tx& tx) noexcept {
            tx.cleanup();
        }
        
        template<typename Tx>
        LSTM_ALWAYS_INLINE static void tx_success(Tx& tx, thread_data& tls_td) noexcept {
            tls_td.access_unlock();
            tls_td.tx = nullptr;
            tx.commit_reclaim();
            tx.reset_heap();
        }
        
        template<typename Func, typename Alloc, std::size_t ReadSize, std::size_t WriteSize,
            std::size_t DeleteSize>
        static transact_result<Func> atomic_slow_path(Func& func,
                                                      transaction_domain& domain,
                                                      const Alloc& alloc,
                                                      knobs<ReadSize, WriteSize, DeleteSize>,
                                                      thread_data& tls_td) {
            tx_result_buffer<transact_result<Func>> buf;
            transaction_impl<Alloc, ReadSize, WriteSize, DeleteSize> tx{domain, tls_td, alloc};
            
            tls_td.tx = &tx;
            
            while(true) {
                tx.reset_read_version();
                try {
                    assert(tx.read_set.size() == 0);
                    assert(tx.write_set.size() == 0);
                    
                    atomic_fn::try_transact(func, tx, buf);
                    
                    // commit does not throw
                    if (tx.commit()) {
                        tx_success(tx, tls_td);
                        return return_tx_result_buffer_fn{}(buf);
                    }
                    buf.reset();
                } catch(const _tx_retry&) {
                    // nothing
                } catch(...) {
                    unhandled_exception(tx, tls_td);
                }
                tx_internal_failed(tx);
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
        transact_result<Func> operator()(Func&& func,
                                         transaction_domain& domain = default_domain(),
                                         const Alloc& alloc = {},
                                         knobs<MaxStackWriteBuffSize,
                                               MaxStackReadBuffSize,
                                               MaxStackDeleterBuffSize> knobs = {},
                                         thread_data& tls_td = tls_thread_data()) const {
            if (tls_td.tx)
                return call(func, *tls_td.tx);
            
            return atomic_fn::atomic_slow_path(func, domain, alloc, knobs, tls_td);
        }
        
#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func, typename... Args,
            LSTM_REQUIRES_(!detail::is_transact_function<Func>())>
        void operator()(Func&&, Args&&...) const {
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

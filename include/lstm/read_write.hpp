#ifndef LSTM_READ_WRITE_HPP
#define LSTM_READ_WRITE_HPP

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    struct read_write_fn
    {
    private:
        template<typename Func, typename Tx, LSTM_REQUIRES_(callable_with_tx<Func, Tx&>())>
        static transact_result<Func> call(Func& func, Tx& tx)
        {
            return func(tx);
        }

        template<typename Func, typename Tx, LSTM_REQUIRES_(!callable_with_tx<Func, Tx&>())>
        static transact_result<Func> call(Func& func, Tx&)
        {
            return func();
        }

        template<typename Tx>
        [[noreturn]] static void unhandled_exception(Tx& tx, thread_data& tls_td)
        {
            tls_td.access_unlock();
            tx.cleanup();
            tls_td.tx = nullptr;
            throw;
        }

        template<typename Tx>
        LSTM_ALWAYS_INLINE static void tx_internal_failed(Tx& tx) noexcept
        {
            tx.cleanup();
            tx.reset_read_version();
        }

        template<typename Tx>
        LSTM_ALWAYS_INLINE static void tx_success(Tx& tx, thread_data& tls_td) noexcept
        {
            tls_td.access_unlock();
            tls_td.tx = nullptr;
            tx.commit_reclaim();
            tx.reset_heap();
        }

        template<typename Func, LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        static transact_result<Func>
        slow_path(Func& func, transaction_domain& domain, thread_data& tls_td)
        {
            transaction tx{domain, tls_td};
            tls_td.tx = &tx;

            while (true) {
                try {
                    assert(tls_td.read_set.size() == 0);
                    assert(tls_td.write_set.size() == 0);
                    assert(tls_td.fail_callbacks.size() == 0);
                    assert(tls_td.succ_callbacks.size() == 0);

                    transact_result<Func> result = read_write_fn::call(func, tx);

                    // commit does not throw
                    if (tx.commit()) {
                        tx_success(tx, tls_td);

                        if (std::is_reference<transact_result<Func>>{})
                            return static_cast<transact_result<Func>&&>(result);
                        else
                            return result;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception(tx, tls_td);
                }
                tx_internal_failed(tx);

                // TODO: add backoff here?
            }
        }

        template<typename Func, LSTM_REQUIRES_(is_void_transact_function<Func>())>
        static void slow_path(Func& func, transaction_domain& domain, thread_data& tls_td)
        {
            transaction tx{domain, tls_td};
            tls_td.tx = &tx;

            while (true) {
                try {
                    assert(tls_td.read_set.size() == 0);
                    assert(tls_td.write_set.size() == 0);
                    assert(tls_td.fail_callbacks.size() == 0);
                    assert(tls_td.succ_callbacks.size() == 0);

                    read_write_fn::call(func, tx);

                    // commit does not throw
                    if (tx.commit()) {
                        tx_success(tx, tls_td);
                        return;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception(tx, tls_td);
                }
                tx_internal_failed(tx);

                // TODO: add backoff here?
            }
        }

    public:
        template<typename Func,
                 LSTM_REQUIRES_(is_transact_function<Func>()
                                && !is_nothrow_transact_function<Func>())>
        transact_result<Func> operator()(Func&&              func,
                                         transaction_domain& domain = default_domain(),
                                         thread_data&        tls_td = tls_thread_data()) const
        {
            if (tls_td.tx)
                return read_write_fn::call(func, *tls_td.tx);

            return read_write_fn::slow_path(func, domain, tls_td);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func>()
                                || is_nothrow_transact_function<Func>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert(is_transact_function<Func>(),
                          "functions passed to lstm::read_write must either take no parameters, "
                          "lstm::transaction&, or auto&/T&");
            static_assert(!is_nothrow_transact_function<Func>(),
                          "functions passed to lstm::read_write must not be marked noexcept");
        }
#endif
    };

    template<typename T>
    constexpr const T static_const{};
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& read_write = detail::static_const<detail::read_write_fn>;
    }
LSTM_END

#endif /* LSTM_READ_WRITE_HPP */

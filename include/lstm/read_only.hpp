#ifndef LSTM_READ_ONLY_HPP
#define LSTM_READ_ONLY_HPP

#include <lstm/detail/atomic_base.hpp>

#include <lstm/read_transaction.hpp>

LSTM_DETAIL_BEGIN
    struct read_only_fn : private detail::atomic_base_fn
    {
    private:
        template<typename Func,
                 LSTM_REQUIRES_(!is_void_transact_function<Func&, read_transaction>())>
        static transact_result<Func, read_transaction>
        slow_path(Func func, transaction_domain& domain, thread_data& tls_td)
        {
            read_transaction tx{tx_start<tx_kind::read_only>(tls_td, domain)};

            while (true) {
                try {
                    LSTM_ASSERT(valid_start_state(tls_td));

                    transact_result<Func, read_transaction> result = atomic_base_fn::call(func, tx);

                    // commit does not throw
                    tx_success<tx_kind::read_only>(tls_td, 0);
                    LSTM_ASSERT(valid_start_state(tls_td));
                    LSTM_ASSERT(!tls_td.in_critical_section());

                    if (std::is_reference<transact_result<Func, read_transaction>>{})
                        return static_cast<transact_result<Func, read_transaction>&&>(result);
                    else
                        return result;
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception<tx_kind::read_only>(tls_td);
                }
                tx.reset_version(tx_restart<tx_kind::read_only>(tls_td, domain));

                // TODO: add backoff here?
            }
        }

        template<typename Func,
                 LSTM_REQUIRES_(is_void_transact_function<Func&, read_transaction>())>
        static void slow_path(Func func, transaction_domain& domain, thread_data& tls_td)
        {
            read_transaction tx{tx_start<tx_kind::read_only>(tls_td, domain)};

            while (true) {
                try {
                    LSTM_ASSERT(valid_start_state(tls_td));

                    atomic_base_fn::call(func, tx);

                    // commit does not throw
                    tx_success<tx_kind::read_only>(tls_td, 0);
                    LSTM_ASSERT(valid_start_state(tls_td));
                    LSTM_ASSERT(!tls_td.in_critical_section());

                    return;
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception<tx_kind::read_only>(tls_td);
                }
                tx.reset_version(tx_restart<tx_kind::read_only>(tls_td, domain));

                // TODO: add backoff here?
            }
        }

    public:
        template<
            typename Func,
            LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction>()
                           && is_transact_function<uncvref<Func>&, read_transaction>()),
            LSTM_REQUIRES_(!is_nothrow_transact_function<Func&&, read_transaction>()
                           && !is_nothrow_transact_function<uncvref<Func>&, read_transaction>())>
        transact_result<Func, read_transaction>
        operator()(Func&&              func,
                   transaction_domain& domain = default_domain(),
                   thread_data&        tls_td = tls_thread_data()) const
        {
            switch (tls_td.tx_kind()) {
            case tx_kind::read_only:
                return atomic_base_fn::call((Func &&) func, read_transaction{tls_td.gp()});
            case tx_kind::read_write:
                return atomic_base_fn::call((Func &&) func, read_transaction{tls_td, tls_td.gp()});
            case tx_kind::none:
                return read_only_fn::slow_path((Func &&) func, domain, tls_td);
            }
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, read_transaction>()
                                || !is_transact_function<uncvref<Func>&, read_transaction>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert(is_transact_function<Func&&, read_transaction>()
                              && !is_transact_function<uncvref<Func>&, read_transaction>(),
                          "functions passed to lstm::read_only must either take no parameters, "
                          "or take a `lstm::read_transaction` either by value or `const&`");
        }

        template<
            typename Func,
            LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction>()
                           && is_transact_function<uncvref<Func>&, read_transaction>()),
            LSTM_REQUIRES_(is_nothrow_transact_function<Func&&, read_transaction>()
                           || is_nothrow_transact_function<uncvref<Func>&, read_transaction>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert(!is_nothrow_transact_function<Func&&, read_transaction>()
                              && !is_nothrow_transact_function<uncvref<Func>&, read_transaction>(),
                          "functions passed to lstm::read_only must not be marked noexcept");
        }
#endif
    };
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& read_only = detail::static_const<detail::read_only_fn>;
    }
LSTM_END

#endif /* LSTM_READ_ONLY_HPP */
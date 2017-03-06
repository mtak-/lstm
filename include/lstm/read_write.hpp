#ifndef LSTM_READ_WRITE_HPP
#define LSTM_READ_WRITE_HPP

#include <lstm/detail/atomic_base.hpp>
#include <lstm/detail/commit_algorithm.hpp>

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    struct read_write_fn : private detail::atomic_base_fn
    {
    private:
        template<typename Func, LSTM_REQUIRES_(!is_void_transact_function<Func&, transaction>())>
        static transact_result<Func, transaction>
        slow_path(Func func, transaction_domain& domain, thread_data& tls_td)
        {
            transaction tx{tls_td, tx_start<tx_kind::read_write>(tls_td, domain)};

            while (true) {
                try {
                    LSTM_ASSERT(valid_start_state(tls_td));

                    transact_result<Func, transaction> result = atomic_base_fn::call(func, tx);

                    // commit does not throw
                    gp_t sync_version;
                    if ((sync_version = detail::commit_algorithm::try_commit(tx, domain))
                        != commit_failed) {
                        tx_success<tx_kind::read_write>(tls_td, sync_version);
                        LSTM_ASSERT(valid_start_state(tls_td));
                        LSTM_ASSERT(!tls_td.in_critical_section());

                        if (std::is_reference<transact_result<Func, transaction>>{})
                            return static_cast<transact_result<Func, transaction>&&>(result);
                        else
                            return result;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception<tx_kind::read_write>(tls_td);
                }
                tx.reset_version(tx_restart<tx_kind::read_write>(tls_td, domain));

                // TODO: add backoff here?
            }
        }

        template<typename Func, LSTM_REQUIRES_(is_void_transact_function<Func&, transaction>())>
        static void slow_path(Func func, transaction_domain& domain, thread_data& tls_td)
        {
            transaction tx{tls_td, tx_start<tx_kind::read_write>(tls_td, domain)};

            while (true) {
                try {
                    LSTM_ASSERT(valid_start_state(tls_td));

                    atomic_base_fn::call(func, tx);

                    // commit does not throw
                    gp_t sync_version;
                    if ((sync_version = detail::commit_algorithm::try_commit(tx, domain))
                        != commit_failed) {
                        tx_success<tx_kind::read_write>(tls_td, sync_version);
                        LSTM_ASSERT(valid_start_state(tls_td));
                        LSTM_ASSERT(!tls_td.in_critical_section());

                        return;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception<tx_kind::read_write>(tls_td);
                }
                tx.reset_version(tx_restart<tx_kind::read_write>(tls_td, domain));

                // TODO: add backoff here?
            }
        }

    public:
        template<typename Func,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction>()
                                && is_transact_function<uncvref<Func>&, transaction>()),
                 LSTM_REQUIRES_(!is_nothrow_transact_function<Func&&, transaction>()
                                && !is_nothrow_transact_function<uncvref<Func>&, transaction>())>
        transact_result<Func, transaction> operator()(Func&& func,
                                                      transaction_domain& domain = default_domain(),
                                                      thread_data& tls_td = tls_thread_data()) const
        {
            LSTM_ASSERT(!tls_td.in_transaction() || tls_td.in_read_write_transaction());
            if (tls_td.in_transaction())
                return atomic_base_fn::call((Func &&) func, transaction{tls_td, tls_td.gp()});

            return read_write_fn::slow_path((Func &&) func, domain, tls_td);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction>()
                                || !is_transact_function<uncvref<Func>&, transaction>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert(is_transact_function<Func&&, transaction>()
                              && !is_transact_function<uncvref<Func>&, transaction>(),
                          "functions passed to lstm::read_write must either take no parameters, "
                          "or take a `lstm::transaction` either by value or `const&`");
        }

        template<typename Func,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction>()
                                && is_transact_function<uncvref<Func>&, transaction>()),
                 LSTM_REQUIRES_(is_nothrow_transact_function<Func&&, transaction>()
                                || is_nothrow_transact_function<uncvref<Func>&, transaction>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert(!is_nothrow_transact_function<Func&&, transaction>()
                              && !is_nothrow_transact_function<uncvref<Func>&, transaction>(),
                          "functions passed to lstm::read_write must not be marked noexcept");
        }
#endif
    };
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& read_write = detail::static_const<detail::read_write_fn>;
    }
LSTM_END

#endif /* LSTM_READ_WRITE_HPP */

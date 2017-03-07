#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/read_only.hpp>
#include <lstm/read_write.hpp>

LSTM_DETAIL_BEGIN
    struct atomic_fn
    {
        template<typename Func, LSTM_REQUIRES_(is_transact_function<Func&&, transaction>())>
        LSTM_ALWAYS_INLINE transact_result<Func, transaction>
        operator()(Func&&              func,
                   transaction_domain& domain = default_domain(),
                   thread_data&        tls_td = tls_thread_data()) const
        {
            LSTM_ASSERT(!tls_td.in_transaction() || tls_td.in_read_write_transaction());
            return ::lstm::read_write((Func &&) func, domain, tls_td);
        }

        template<typename Func,
                 LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction>()
                                && !is_transact_function<Func&&, transaction>())>
        LSTM_ALWAYS_INLINE transact_result<Func, read_transaction>
        operator()(Func&&              func,
                   transaction_domain& domain = default_domain(),
                   thread_data&        tls_td = tls_thread_data()) const
        {
            return ::lstm::read_only((Func &&) func, domain, tls_td);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction>()
                                && !is_transact_function<Func&&, read_transaction>())>
        void operator()(Func&&,
                        transaction_domain& = default_domain(),
                        thread_data&        = tls_thread_data()) const
        {
            static_assert((is_transact_function_<Func&&, transaction>()
                           && is_transact_function_<uncvref<Func>&, transaction>())
                              || (is_transact_function_<Func&&, read_transaction>()
                                  && is_transact_function_<uncvref<Func>&, read_transaction>()),
                          "functions passed to lstm::atomic must either take no parameters, "
                          "or take a `lstm::transaction` or `lstm::read_transaction` either by "
                          "value or `const&`");
            static_assert(!is_nothrow_transact_function<Func&&, transaction>()
                              && !is_nothrow_transact_function<uncvref<Func>&, transaction>()
                              && !is_nothrow_transact_function<Func&&, read_transaction>()
                              && !is_nothrow_transact_function<uncvref<Func>&, read_transaction>(),
                          "functions passed to lstm::atomic must not be marked noexcept");
        }
#endif
    };
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& atomic = detail::static_const<detail::atomic_fn>;
    }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
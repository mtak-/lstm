#ifndef LSTM_ATOMIC_HPP
#define LSTM_ATOMIC_HPP

#include <lstm/read_only.hpp>
#include <lstm/read_write.hpp>

LSTM_DETAIL_BEGIN
    struct atomic_fn
    {
        /*************
         * read_write
         *************/
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, transaction, Args&&...>
        operator()(thread_data&        tls_td,
                   transaction_domain& domain,
                   Func&&              func,
                   Args&&... args) const
        {
            LSTM_ASSERT(!tls_td.in_transaction() || tls_td.in_read_write_transaction());
            return ::lstm::read_write(tls_td, domain, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, transaction, Args&&...>
        operator()(thread_data& tls_td, Func&& func, Args&&... args) const
        {
            LSTM_ASSERT(!tls_td.in_transaction() || tls_td.in_read_write_transaction());
            return ::lstm::read_write(tls_td, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, transaction, Args&&...>
        operator()(transaction_domain& domain, Func&& func, Args&&... args) const
        {
            return ::lstm::read_write(domain, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, transaction, Args&&...>
        operator()(Func&& func, Args&&... args) const
        {
            return ::lstm::read_write((Func &&) func, (Args &&) args...);
        }

        /************
         * read_only
         ************/
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction, Args&&...>()
                                && !is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, read_transaction, Args&&...>
        operator()(thread_data&        tls_td,
                   transaction_domain& domain,
                   Func&&              func,
                   Args&&... args) const
        {
            return ::lstm::read_only(tls_td, domain, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction, Args&&...>()
                                && !is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, read_transaction, Args&&...>
        operator()(thread_data& tls_td, Func&& func, Args&&... args) const
        {
            return ::lstm::read_only(tls_td, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction, Args&&...>()
                                && !is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, read_transaction, Args&&...>
        operator()(transaction_domain& domain, Func&& func, Args&&... args) const
        {
            return ::lstm::read_only(domain, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, read_transaction, Args&&...>()
                                && !is_transact_function<Func&&, transaction, Args&&...>())>
        LSTM_ALWAYS_INLINE transact_result<Func, read_transaction, Args&&...>
        operator()(Func&& func, Args&&... args) const
        {
            return ::lstm::read_only((Func &&) func, (Args &&) args...);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction, Args&&...>()
                                && !is_transact_function<Func&&, read_transaction, Args&&...>())>
        void operator()(thread_data&, transaction_domain&, Func&&, Args&&...) const
        {
            static_assert((is_transact_function_<Func&&, transaction, Args&&...>()
                           && is_transact_function_<uncvref<Func>&, transaction, Args&&...>())
                              || (is_transact_function_<Func&&, read_transaction, Args&&...>()
                                  && is_transact_function_<uncvref<Func>&,
                                                           read_transaction,
                                                           Args&&...>()),
                          "functions passed to lstm::atomic must either take no parameters, "
                          "or take a `lstm::transaction` or `lstm::read_transaction` either by "
                          "value or `const&`");
            static_assert(!is_nothrow_transact_function<Func&&, transaction, Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<Func&&,
                                                               read_transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               read_transaction,
                                                               Args&&...>(),
                          "functions passed to lstm::atomic must not be marked noexcept");
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction, Args&&...>()
                                && !is_transact_function<Func&&, read_transaction, Args&&...>())>
        void operator()(thread_data&, Func&&, Args&&...) const
        {
            static_assert((is_transact_function_<Func&&, transaction, Args&&...>()
                           && is_transact_function_<uncvref<Func>&, transaction, Args&&...>())
                              || (is_transact_function_<Func&&, read_transaction, Args&&...>()
                                  && is_transact_function_<uncvref<Func>&,
                                                           read_transaction,
                                                           Args&&...>()),
                          "functions passed to lstm::atomic must either take no parameters, "
                          "or take a `lstm::transaction` or `lstm::read_transaction` either by "
                          "value or `const&`");
            static_assert(!is_nothrow_transact_function<Func&&, transaction, Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<Func&&,
                                                               read_transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               read_transaction,
                                                               Args&&...>(),
                          "functions passed to lstm::atomic must not be marked noexcept");
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction, Args&&...>()
                                && !is_transact_function<Func&&, read_transaction, Args&&...>())>
        void operator()(transaction_domain&, Func&&, Args&&...) const
        {
            static_assert((is_transact_function_<Func&&, transaction, Args&&...>()
                           && is_transact_function_<uncvref<Func>&, transaction, Args&&...>())
                              || (is_transact_function_<Func&&, read_transaction, Args&&...>()
                                  && is_transact_function_<uncvref<Func>&,
                                                           read_transaction,
                                                           Args&&...>()),
                          "functions passed to lstm::atomic must either take no parameters, "
                          "or take a `lstm::transaction` or `lstm::read_transaction` either by "
                          "value or `const&`");
            static_assert(!is_nothrow_transact_function<Func&&, transaction, Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<Func&&,
                                                               read_transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               read_transaction,
                                                               Args&&...>(),
                          "functions passed to lstm::atomic must not be marked noexcept");
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, transaction, Args&&...>()
                                && !is_transact_function<Func&&, read_transaction, Args&&...>())>
        void operator()(Func&&, Args&&...) const
        {
            static_assert((is_transact_function_<Func&&, transaction, Args&&...>()
                           && is_transact_function_<uncvref<Func>&, transaction, Args&&...>())
                              || (is_transact_function_<Func&&, read_transaction, Args&&...>()
                                  && is_transact_function_<uncvref<Func>&,
                                                           read_transaction,
                                                           Args&&...>()),
                          "functions passed to lstm::atomic must either take no parameters, "
                          "or take a `lstm::transaction` or `lstm::read_transaction` either by "
                          "value or `const&`");
            static_assert(!is_nothrow_transact_function<Func&&, transaction, Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<Func&&,
                                                               read_transaction,
                                                               Args&&...>()
                              && !is_nothrow_transact_function<uncvref<Func>&,
                                                               read_transaction,
                                                               Args&&...>(),
                          "functions passed to lstm::atomic must not be marked noexcept");
        }
#endif /* LSTM_MAKE_SFINAE_FRIENDLY */
    };
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& atomic = detail::static_const<detail::atomic_fn>;
    }
LSTM_END

#endif /* LSTM_ATOMIC_HPP */
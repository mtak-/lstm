#ifndef LSTM_RELATIVE_HPP
#define LSTM_RELATIVE_HPP

#include <lstm/detail/atomic_base.hpp>

#include <lstm/critical_section.hpp>

LSTM_DETAIL_BEGIN
    struct relative_fn : private detail::atomic_base_fn
    {
    private:
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_void_transact_function<Func&, critical_section, Args&&...>()),
                 typename Result = transact_result<Func, critical_section, Args&&...>>
        static Result slow_path(thread_data& tls_td, Func func, Args&&... args)
        {
            LSTM_ASSERT(!tls_td.in_transaction());
            LSTM_ASSERT(valid_start_state(tls_td));
            try {
                tls_td.access_lock(domain.get_clock());

                Result result = atomic_base_fn::call(func, critical_section{}, (Args &&) args...);

                tls_td.access_unlock();
                LSTM_ASSERT(valid_start_state(tls_td));

                if (std::is_reference<Result>{})
                    return static_cast<Result>(result);
                else
                    return result;
            } catch (...) {
                tls_td.access_unlock();
                LSTM_ASSERT(valid_start_state(tls_td));
                throw;
            }
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_void_transact_function<Func&, critical_section, Args&&...>())>
        static void slow_path(thread_data& tls_td, domain, Func func, Args&&... args)
        {
            LSTM_ASSERT(!tls_td.in_transaction());
            LSTM_ASSERT(valid_start_state(tls_td));
            try {
                tls_td.access_lock(domain.get_clock());

                atomic_base_fn::call(func, critical_section{}, (Args &&) args...);

                tls_td.access_unlock();
                LSTM_ASSERT(valid_start_state(tls_td));
            } catch (...) {
                tls_td.access_unlock();
                LSTM_ASSERT(valid_start_state(tls_td));
                throw;
            }
        }

    public:
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, critical_section, Args&&...>())>
        transact_result<Func, critical_section, Args&&...>
        operator()(thread_data& tls_td, Func&& func, Args&&... args) const
        {
            if (tls_td.in_critical_section())
                return atomic_base_fn::call(func, critical_section{}, (Args &&) args...);
            return relative_fn::slow_path(tls_td, domain, (Func &&) func, (Args &&) args...);
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(is_transact_function<Func&&, critical_section, Args&&...>())>
        transact_result<Func, critical_section, Args&&...>
        operator()(Func&& func, Args&&... args) const
        {
            return (*this)(tls_thread_data(), default_domain(), (Func &&) func, (Args &&) args...);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, critical_section, Args&&...>())>
        transact_result<Func, critical_section, Args&&...>
        operator()(thread_data&, Func&&, Args&&...) const
        {
            static_assert(is_transact_function_<Func&&, critical_section, Args&&...>()
                              && is_transact_function_<uncvref<Func>&,
                                                       critical_section,
                                                       Args&&...>(),
                          "functions passed to lstm::relative must either take no parameters, "
                          "or take a `lstm::critical_section` either by value or `const&`");
        }

        template<typename Func,
                 typename... Args,
                 LSTM_REQUIRES_(!is_transact_function<Func&&, critical_section, Args&&...>())>
        transact_result<Func, critical_section, Args&&...> operator()(Func&&, Args&&...) const
        {
            static_assert(is_transact_function_<Func&&, critical_section, Args&&...>()
                              && is_transact_function_<uncvref<Func>&,
                                                       critical_section,
                                                       Args&&...>(),
                          "functions passed to lstm::relative must either take no parameters, "
                          "or take a `lstm::critical_section` either by value or `const&`");
        }
#endif /* LSTM_MAKE_SFINAE_FRIENDLY */
    };
LSTM_DETAIL_END

LSTM_BEGIN
    namespace
    {
        constexpr auto& relative = detail::static_const<detail::relative_fn>;
    }
LSTM_END

#endif /* LSTM_RELATIVE_HPP */
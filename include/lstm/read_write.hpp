#ifndef LSTM_READ_WRITE_HPP
#define LSTM_READ_WRITE_HPP

#include <lstm/detail/commit_algorithm.hpp>

#include <lstm/transaction.hpp>

LSTM_DETAIL_BEGIN
    struct read_write_fn
    {
    private:
        template<typename Func, LSTM_REQUIRES_(callable_with_tx<Func, transaction&>())>
        static transact_result<Func> call(Func& func, transaction tx)
        {
            return func(tx);
        }

        template<typename Func, LSTM_REQUIRES_(!callable_with_tx<Func, transaction&>())>
        static transact_result<Func> call(Func& func, const transaction)
        {
            return func();
        }

        static void thread_data_failed(thread_data& tls_td) noexcept
        {
            tls_td.write_set.clear();
            tls_td.read_set.clear();
            tls_td.succ_callbacks.clear();
            tls_td.do_fail_callbacks();
        }

        [[noreturn]] static void unhandled_exception(thread_data& tls_td)
        {
            tls_td.access_unlock();
            thread_data_failed(tls_td);
            tls_td.in_transaction_ = false;
            throw;
        }

        LSTM_ALWAYS_INLINE static void
        tx_failed(transaction& tx, transaction_domain& domain) noexcept
        {
            thread_data& tls_td = tx.get_thread_data();
            thread_data_failed(tls_td);
            const gp_t new_version = domain.get_clock();
            tls_td.access_relock(new_version);
            tx.reset_version(new_version);
        }

        static bool valid_start_state(thread_data& tls_td) noexcept
        {
            return tls_td.read_set.empty() && tls_td.write_set.empty()
                   && tls_td.fail_callbacks.empty() && tls_td.succ_callbacks.empty();
        }

        static void tx_success(thread_data& tls_td, const gp_t sync_version) noexcept
        {
            tls_td.access_unlock();
            tls_td.in_transaction_ = false;
            tls_td.write_set.clear();
            tls_td.read_set.clear();
            tls_td.fail_callbacks.clear();
            tls_td.reclaim(sync_version);
        }

        template<typename Func, LSTM_REQUIRES_(!is_void_transact_function<Func>())>
        static transact_result<Func>
        slow_path(Func& func, transaction_domain& domain, thread_data& tls_td)
        {
            const gp_t version = domain.get_clock();
            tls_td.access_lock(version);
            transaction tx{tls_td, version};
            tls_td.in_transaction_ = true;

            while (true) {
                try {
                    assert(valid_start_state(tls_td));

                    transact_result<Func> result = read_write_fn::call(func, tx);

                    // commit does not throw
                    gp_t sync_version;
                    if ((sync_version = detail::commit_algorithm::try_commit(tx, domain))
                        != commit_failed) {
                        tx_success(tls_td, sync_version);
                        assert(valid_start_state(tls_td));
                        assert(!tls_td.in_critical_section());

                        if (std::is_reference<transact_result<Func>>{})
                            return static_cast<transact_result<Func>&&>(result);
                        else
                            return result;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception(tls_td);
                }
                tx_failed(tx, domain);

                // TODO: add backoff here?
            }
        }

        template<typename Func, LSTM_REQUIRES_(is_void_transact_function<Func>())>
        static void slow_path(Func& func, transaction_domain& domain, thread_data& tls_td)
        {
            const gp_t version = domain.get_clock();
            tls_td.access_lock(version);
            transaction tx{tls_td, version};
            tls_td.in_transaction_ = true;

            while (true) {
                try {
                    assert(valid_start_state(tls_td));

                    read_write_fn::call(func, tx);

                    // commit does not throw
                    gp_t sync_version;
                    if ((sync_version = detail::commit_algorithm::try_commit(tx, domain))
                        != commit_failed) {
                        tx_success(tls_td, sync_version);
                        assert(valid_start_state(tls_td));
                        assert(!tls_td.in_critical_section());

                        return;
                    }
                } catch (const tx_retry&) {
                    // nothing
                } catch (...) {
                    unhandled_exception(tls_td);
                }
                tx_failed(tx, domain);

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
            if (tls_td.in_transaction())
                return read_write_fn::call(func, {tls_td, tls_td.active.load(LSTM_RELAXED)});

            return read_write_fn::slow_path(func, domain, tls_td);
        }

#ifndef LSTM_MAKE_SFINAE_FRIENDLY
        template<typename Func,
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

#ifndef LSTM_DETAIL_BACKOFF_HPP
#define LSTM_DETAIL_BACKOFF_HPP

#include <lstm/detail/lstm_fwd.hpp>

// clang-format off
#ifndef LSTM_USE_BOOST_FIBERS
    #include <thread>
    #define LSTM_THIS_CONTEXT std::this_thread
#else
    #include <boost/fiber/operations.hpp>
    #define LSTM_THIS_CONTEXT boost::this_fiber
#endif
// clang-format on

LSTM_DETAIL_BEGIN
    template<typename T>
    using has_reset_ = decltype(std::declval<T&>().reset());

    template<typename T>
    using has_reset = supports<has_reset_, T>;

    template<typename T>
    using has_nullary_call_operator_ = decltype(std::declval<T&>().operator()());

    template<typename T>
    using has_nullary_call_operator = supports<has_nullary_call_operator_, T>;

    template<typename T>
    using is_backoff_strategy = and_<std::is_trivially_copy_constructible<T>,
                                     std::is_trivially_move_constructible<T>,
                                     std::is_trivially_destructible<T>,
                                     std::is_standard_layout<T>,
                                     has_reset<T>,
                                     has_nullary_call_operator<T>>;

    template<typename Interval, std::size_t Min, std::size_t Max>
    struct exponential_delay
    {
    private:
        std::size_t interval{Min};
        static_assert(Min <= Max, "Min delay must be less than or equal to Max delay");
        static_assert(!(Max & (std::size_t(1) << (sizeof(std::size_t) * 8 - 1))),
                      "The number chosen for Max delay runs the risk of overflow.");

    public:
        void operator()() noexcept
        {
            LSTM_THIS_CONTEXT::sleep_for(Interval(interval));
            interval <<= 1;
            if (interval > Max)
                interval = Max;
        }

        void reset() noexcept { interval = Min; }
    };

    struct yield
    {
        // noinline, cause yield on libc++ is inline, and calls a non-noexcept func
        LSTM_NOINLINE_LUKEWARM void operator()() const noexcept { LSTM_THIS_CONTEXT::yield(); }
        LSTM_ALWAYS_INLINE void     reset() const noexcept {}
    };

    using default_backoff = yield;
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_BACKOFF_HPP */
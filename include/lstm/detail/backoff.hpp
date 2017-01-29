#ifndef LSTM_DETAIL_BACKOFF_HPP
#define LSTM_DETAIL_BACKOFF_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <thread>

LSTM_DETAIL_BEGIN
    template<typename T, typename = void> struct has_reset : std::false_type {};
    template<typename T> struct has_reset<T,
        void_<decltype(std::declval<T&>().reset())>>
        : std::integral_constant<bool, noexcept(std::declval<T&>().reset())> {};
    
    template<typename T, typename = void> struct has_nullary_call_operator : std::false_type {};
    template<typename T> struct has_nullary_call_operator<T,
        void_<decltype(std::declval<T&>().operator()())>>
        : std::integral_constant<bool, noexcept(std::declval<T&>().operator()())> {};
    
    template<typename T>
    using is_backoff_strategy = and_<
            std::is_trivially_copy_constructible<T>,
            std::is_trivially_move_constructible<T>,
            std::is_trivially_destructible<T>,
            std::is_standard_layout<T>,
            has_reset<T>,
            has_nullary_call_operator<T>>;
    
    template<typename Interval, std::size_t Min, std::size_t Max>
    struct exponential_delay {
    private:
        std::size_t interval{Min};
        static_assert(Min <= Max, "Min delay must be less than or equal to Max delay");
        static_assert(!(Max & (std::size_t(1) << (sizeof(std::size_t) * 8 - 1))),
            "The number chosen for Max delay runs the risk of overflow.");
        
    public:
        void operator()() noexcept {
            std::this_thread::sleep_for(Interval(interval));
            interval <<= 1;
            if (interval > Max) interval = Max;
        }
        
        void reset() noexcept { interval = Min; }
    };
    
    static_assert(
            is_backoff_strategy<exponential_delay<std::chrono::nanoseconds, 100000, 10000000>>{},
            "");
    
    struct yield {
        LSTM_ALWAYS_INLINE void operator()() const noexcept
        { std::this_thread::yield(); }
        
        LSTM_ALWAYS_INLINE void reset() const noexcept {}
    };
    
    static_assert(is_backoff_strategy<yield>{}, "");
    
    using default_backoff = exponential_delay<std::chrono::nanoseconds, 100000, 10000000>;
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_BACKOFF_HPP */
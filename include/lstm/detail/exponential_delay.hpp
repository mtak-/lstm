#ifndef LSTM_DETAIL_EXPONENTIAL_DELAY_HPP
#define LSTM_DETAIL_EXPONENTIAL_DELAY_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <thread>

LSTM_DETAIL_BEGIN
    template<std::size_t Min_ns, std::size_t Max_ns>
    struct exponential_delay {
    private:
        std::size_t ns{Min_ns};
        static_assert(Min_ns <= Max_ns &&
            !(Max_ns & (std::size_t(1) << (sizeof(std::size_t) * 8 - 1))), "");
        
    public:
        void operator()() noexcept {
            std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
            ns <<= 1;
            if (ns > Max_ns) ns = Max_ns;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_EXPONENTIAL_DELAY_HPP */
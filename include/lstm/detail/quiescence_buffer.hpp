#ifndef LSTM_DETAIL_QUIESCENCE_BUFFER_HPP
#define LSTM_DETAIL_QUIESCENCE_BUFFER_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_vector.hpp>

LSTM_DETAIL_BEGIN
    struct quiescence_callback
    {
        epoch_t                 epoch;
        pod_vector<gp_callback> callbacks;
    };

    template<std::int8_t RingSize>
    struct quiescence_buffer
    {
    private:
        quiescence_callback callbacks[RingSize];
        std::int8_t         start{0};
        std::int8_t         end{0};

    public:
        quiescence_callback& active() noexcept { return callbacks[end]; }
        quiescence_callback& front() noexcept { return callbacks[start]; }
        quiescence_callback& back() noexcept { return callbacks[(end - 1 + RingSize) % RingSize]; }

        bool push_is_full(const epoch_t epoch) noexcept
        {
            LSTM_ASSERT(!active().callbacks.empty());
            active().epoch = epoch;

            end = (end + 1) % RingSize;

            const bool result = end == start;
            LSTM_ASSERT(active().callbacks.empty() || result);
            return result;
        }

        bool empty() const noexcept { return start == end; }

        void pop_front() noexcept { start = (start + 1) % RingSize; }

        void shrink_to_fit() noexcept(noexcept(active().callbacks.shrink_to_fit()))
        {
            for (quiescence_callback& succ_callback : callbacks)
                succ_callback.callbacks.shrink_to_fit();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_QUIESCENCE_BUFFER_HPP */

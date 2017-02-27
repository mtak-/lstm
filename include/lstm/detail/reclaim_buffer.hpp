#ifndef LSTM_DETAIL_RECLAIM_BUFFER_HPP
#define LSTM_DETAIL_RECLAIM_BUFFER_HPP

#include <lstm/detail/backoff.hpp>
#include <lstm/detail/gp_callback.hpp>
#include <lstm/detail/pod_vector.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct succ_callback_t
    {
        gp_t                    version;
        pod_vector<gp_callback> callbacks;
    };

    template<std::int8_t StackCount>
    struct succ_callbacks_t
    {
    private:
        succ_callback_t callbacks[StackCount];
        std::int8_t     start{0};
        std::int8_t     end{0};

    public:
        succ_callback_t& active() noexcept { return callbacks[end]; }
        succ_callback_t& front() noexcept { return callbacks[start]; }
        succ_callback_t& back() noexcept { return callbacks[(end - 1 + StackCount) % StackCount]; }

        bool push_is_full(const gp_t version) noexcept
        {
            assert(!active().callbacks.empty());
            active().version = version;

            end = (end + 1) % StackCount;

            const bool result = end == start;
            assert(active().callbacks.empty() || result);
            return result;
        }

        bool empty() const noexcept { return start == end; }

        void pop_front() noexcept
        {
            start = (start + 1) % StackCount;
        }

        void shrink_to_fit() noexcept(noexcept(active().callbacks.shrink_to_fit()))
        {
            for (succ_callback_t& succ_callback : callbacks)
                succ_callback.callbacks.shrink_to_fit();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RECLAIM_BUFFER_HPP */

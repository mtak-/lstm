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

    template<std::size_t StackCount = 4>
    struct succ_callbacks_t
    {
    private:
        succ_callback_t callbacks[StackCount];
        std::int8_t     start{0};
        std::int8_t     size{0};

        std::int8_t active_index() const noexcept { return (start + size) % StackCount; }

    public:
        succ_callback_t& active() noexcept { return callbacks[active_index()]; }
        succ_callback_t& front() noexcept { return callbacks[start]; }
        succ_callback_t& back() noexcept { return callbacks[(start + size - 1) % StackCount]; }

        bool push_is_full(const gp_t version) noexcept
        {
            assert(!active().callbacks.empty());
            active().version = version;

            const bool result = ++size == StackCount;
            assert(active().callbacks.empty() || result);
            return result;
        }

        bool empty() const noexcept { return size == 0; }

        void pop_front() noexcept
        {
            assert(size != 0);
            assert(front().callbacks.empty());
            start = (start + 1) % StackCount;
            --size;
        }

        void shrink_to_fit() noexcept(noexcept(active().callbacks.shrink_to_fit()))
        {
            for (succ_callback_t& succ_callback : callbacks)
                succ_callback.callbacks.shrink_to_fit();
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_RECLAIM_BUFFER_HPP */

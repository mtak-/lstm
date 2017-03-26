#ifndef LSTM_TRANSACTION_DOMAIN_HPP
#define LSTM_TRANSACTION_DOMAIN_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_DETAIL_BEGIN
    struct transaction_domain
    {
    private:
        LSTM_CACHE_ALIGNED std::atomic<epoch_t> clock;

    public:
        inline constexpr transaction_domain() noexcept
            : clock{0}
        {
        }

        transaction_domain(const transaction_domain&) = delete;
        transaction_domain& operator=(const transaction_domain&) = delete;

        inline epoch_t get_clock() const noexcept { return clock.load(LSTM_ACQUIRE); }

        // returns the previous version
        inline epoch_t fetch_and_bump_clock() noexcept
        {
            const epoch_t result = clock.fetch_add(bump_size(), LSTM_RELEASE);
            LSTM_ASSERT(result < max_version() - bump_size());
            return result;
        }

        static inline constexpr epoch_t bump_size() noexcept { return 1; }
        static inline constexpr epoch_t max_version() noexcept
        {
            return ~(epoch_t(1) << (sizeof(epoch_t) * 8 - bump_size()));
        }
    };

    inline transaction_domain& default_domain() noexcept
    {
        static transaction_domain _default_domain{};
        return _default_domain;
    }
LSTM_DETAIL_END

#endif /* LSTM_TRANSACTION_DOMAIN_HPP */

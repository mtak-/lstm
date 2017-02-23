#ifndef LSTM_TRANSACTION_DOMAIN_HPP
#define LSTM_TRANSACTION_DOMAIN_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>
#include <cassert>

LSTM_BEGIN
    struct transaction_domain
    {
    private:
        static constexpr gp_t max_version = ~(gp_t(1) << (sizeof(gp_t) * 8 - 1));

        LSTM_CACHE_ALIGNED std::atomic<gp_t> clock;

    public:
        inline constexpr transaction_domain() noexcept
            : clock{0}
        {
        }

        transaction_domain(const transaction_domain&) = delete;
        transaction_domain& operator=(const transaction_domain&) = delete;

        inline gp_t get_clock() const noexcept { return clock.load(LSTM_ACQUIRE); }

        // returns the previous version
        inline gp_t fetch_and_bump_clock() noexcept
        {
            const gp_t result = clock.fetch_add(1, LSTM_RELEASE);
            assert(result < max_version - 1);
            return result;
        }
    };

    inline transaction_domain& default_domain() noexcept
    {
        static transaction_domain _default_domain{};
        return _default_domain;
    }
LSTM_END

#endif /* LSTM_TRANSACTION_DOMAIN_HPP */

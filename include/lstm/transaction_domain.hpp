#ifndef LSTM_TRANSACTION_DOMAIN_HPP
#define LSTM_TRANSACTION_DOMAIN_HPP

#include <lstm/detail/lstm_fwd.hpp>
#include <lstm/detail/thread_local.hpp>

#include <atomic>
#include <cassert>

LSTM_BEGIN
    struct transaction_domain {
    private:
        static constexpr word clock_bump_size = 2;
        static constexpr word max_version = std::numeric_limits<word>::max() - clock_bump_size + 1;
        
        std::atomic<word> clock{0};
        
        inline word bump_clock() noexcept {
            word result = clock.fetch_add(clock_bump_size, LSTM_RELEASE) + clock_bump_size;
            assert(result < max_version);
            return result;
        }
        
        static transaction_domain& default_domain() noexcept;
        
        template<typename Alloc>
        friend struct lstm::detail::transaction_impl;
        
    public:
        inline transaction_domain() noexcept = default;
        transaction_domain(const transaction_domain&) = delete;
        transaction_domain& operator=(const transaction_domain&) = delete;
        
        inline word get_clock() noexcept { return clock.load(LSTM_ACQUIRE); }
    };
    
    namespace detail {
        // inline
        template<std::nullptr_t = nullptr> transaction_domain _default_domain{};
    }
    
    inline transaction_domain& default_domain() noexcept { return detail::_default_domain<>; }
LSTM_END

#endif /* LSTM_TRANSACTION_DOMAIN_HPP */
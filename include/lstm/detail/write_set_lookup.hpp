#ifndef LSTM_DETAIL_WRITE_SET_LOOKUP_HPP
#define LSTM_DETAIL_WRITE_SET_LOOKUP_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    struct write_set_lookup {
        hash_t hash_;
        var_storage* pending_write_;
        
        inline constexpr bool success() const noexcept { return !hash_; }
        inline constexpr var_storage& pending_write() const noexcept { return *pending_write_; }
        inline constexpr hash_t& hash() noexcept { return hash_; }
        inline constexpr hash_t hash() const noexcept { return hash_; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_WRITE_SET_LOOKUP_HPP */
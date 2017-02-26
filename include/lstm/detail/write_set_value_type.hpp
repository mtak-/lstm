#ifndef LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP
#define LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct write_set_value_type
    {
    private:
        var_base*   dest_var_;
        var_storage pending_write_;

        inline write_set_value_type() noexcept = default;

    public:
        inline constexpr write_set_value_type(var_base* const in_dest_var,
                                              var_storage     in_pending_write) noexcept
            : dest_var_(in_dest_var)
            , pending_write_(std::move(in_pending_write))
        {
            assert(dest_var_);
        }

        inline constexpr var_base& dest_var() const noexcept
        {
            assert(dest_var_);
            return *dest_var_;
        }

        inline constexpr var_storage& pending_write() noexcept
        {
            assert(dest_var_);
            return pending_write_;
        }

        inline constexpr var_storage pending_write() const noexcept
        {
            assert(dest_var_);
            return pending_write_;
        }

        inline constexpr bool is_dest_var(const var_base& rhs) const noexcept
        {
            assert(dest_var_);
            return dest_var_ == &rhs;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP */
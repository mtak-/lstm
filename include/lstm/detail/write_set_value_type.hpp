#ifndef LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP
#define LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    struct write_set_value_type
    {
    private:
        var_base*   dest_var_;
        var_storage pending_write_;

        inline write_set_value_type() noexcept = default;

    public:
        inline write_set_value_type(var_base* const in_dest_var,
                                    var_storage     in_pending_write) noexcept
            : dest_var_(in_dest_var)
            , pending_write_(std::move(in_pending_write))
        {
            LSTM_ASSERT(dest_var_);
        }

        inline var_base& dest_var() const noexcept
        {
            LSTM_ASSERT(dest_var_);
            return *dest_var_;
        }

        inline var_storage& pending_write() noexcept
        {
            LSTM_ASSERT(dest_var_);
            return pending_write_;
        }

        inline var_storage pending_write() const noexcept
        {
            LSTM_ASSERT(dest_var_);
            return pending_write_;
        }

        inline bool is_dest_var(const var_base& rhs) const noexcept
        {
            LSTM_ASSERT(dest_var_);
            return dest_var_ == &rhs;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP */
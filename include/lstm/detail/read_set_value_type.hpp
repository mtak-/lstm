#ifndef LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP
#define LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    struct read_set_value_type
    {
    private:
        const var_base* src_var_;

        inline read_set_value_type() noexcept = default;

    public:
        inline read_set_value_type(const var_base* const in_src_var) noexcept
            : src_var_(in_src_var)
        {
            LSTM_ASSERT(src_var_);
        }

        inline const var_base& src_var() const noexcept
        {
            LSTM_ASSERT(src_var_);
            return *src_var_;
        }

        inline bool is_src_var(const var_base& rhs) const noexcept
        {
            LSTM_ASSERT(src_var_);
            return src_var_ == &rhs;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP */
#ifndef LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP
#define LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct read_set_value_type {
    private:
        const var_base* _src_var;
        
    public:
        inline read_set_value_type() noexcept = default;
        inline constexpr read_set_value_type(const var_base* const in_src_var) noexcept
            : _src_var(in_src_var)
        { assert(_src_var); }
        
        inline constexpr const var_base& src_var() const noexcept {
            assert(_src_var);
            return *_src_var;
        }
        
        inline constexpr bool is_src_var(const var_base& rhs) const noexcept {
            assert(_src_var);
            return _src_var == &rhs;
        }
        
        inline constexpr bool operator<(const read_set_value_type& rhs) const noexcept {
            assert(_src_var && rhs._src_var);
            return _src_var < rhs._src_var;
        }
        
        inline constexpr bool operator==(const read_set_value_type& rhs) const noexcept {
            assert(_src_var && rhs._src_var);
            return _src_var == rhs._src_var;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_READ_SET_VALUE_TYPE_HPP */
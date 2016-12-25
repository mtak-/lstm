#ifndef LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP
#define LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct write_set_value_type {
    private:
        var_base* _dest_var;
        var_storage _pending_write;
        
    public:
        inline write_set_value_type() noexcept = default;
        
        inline constexpr
        write_set_value_type(var_base* const in_dest_var, var_storage in_pending_write) noexcept
            : _dest_var(in_dest_var)
            , _pending_write(std::move(in_pending_write))
        { assert(_dest_var); }
        
        inline constexpr var_base& dest_var() const noexcept {
            assert(_dest_var);
            return *_dest_var;
        }
        
        inline constexpr var_storage& pending_write() noexcept {
            assert(_dest_var);
            return _pending_write;
        }
        
        inline constexpr const var_storage& pending_write() const noexcept {
            assert(_dest_var);
            return _pending_write;
        }
        
        inline constexpr bool is_dest_var(const var_base& rhs) const noexcept {
            assert(_dest_var);
            return _dest_var == &rhs;
        }
        
        inline constexpr bool operator<(const write_set_value_type& rhs) const noexcept {
            assert(_dest_var && rhs._dest_var);
            return _dest_var < rhs._dest_var;
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_WRITE_SET_VALUE_TYPE_HPP */
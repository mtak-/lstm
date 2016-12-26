#ifndef LSTM_DETAIL_WRITE_SET_DELETER_HPP
#define LSTM_DETAIL_WRITE_SET_DELETER_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_DETAIL_BEGIN
    struct write_set_deleter {
        var_base* var_ptr;
        var_storage storage;
        
        inline write_set_deleter() noexcept = default;
        inline constexpr write_set_deleter(var_base* const in_var_ptr,
                                           const var_storage in_storage) noexcept
            : var_ptr(in_var_ptr)
            , storage(in_storage)
        {}
        
        void operator()() const noexcept { var_ptr->destroy_deallocate(storage); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_WRITE_SET_DELETER_HPP */
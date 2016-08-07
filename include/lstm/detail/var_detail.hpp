#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_DETAIL_BEGIN
    struct var_base {
    protected:
        friend transaction;
        
        var_base(void* in_value)
            : version_lock{0}
            , value(in_value)
        {}
            
        virtual void destroy_value() const = 0;
        
        mutable std::atomic<word> version_lock;
        std::atomic<void*> value;
    };

    template<typename T>
    struct var_impl : var_base {
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var_impl(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{})
            : var_base{(void*)new T((Us&&)us...)}
        {}
    
    private:
        void destroy_value() const override
        { delete static_cast<T*>(value.load(LSTM_ACQUIRE)); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
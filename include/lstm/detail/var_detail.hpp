#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_DETAIL_BEGIN
    struct var_base {
    protected:
        template<typename Alloc>
        friend struct ::lstm::transaction;
        
        var_base(void* in_value)
            : version_lock{0}
            , value(in_value)
        {}
            
        virtual void destroy_value(void* ptr) const = 0;
        
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
        
        ~var_impl() noexcept(std::is_nothrow_destructible<T>{})
        { delete static_cast<T*>(value.load(LSTM_RELAXED)); }
    
    private:
        void destroy_value(void* ptr) const override
        { delete static_cast<T*>(ptr); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_MACROS_HPP */
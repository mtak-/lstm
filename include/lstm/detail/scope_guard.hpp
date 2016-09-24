#ifndef LSTM_SCOPE_GUARD_HPP
#define LSTM_SCOPE_GUARD_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    template<typename F>
    struct scope_guard {
    private:
        F f;
        bool engaged;
        
    public:
        scope_guard(F f_in)
            : f(std::move(f_in))
            , engaged(true)
        {}
        
        scope_guard(scope_guard&& rhs)
            : f(std::move(rhs.f))
            , engaged(rhs.engaged)
        { rhs.engaged = false; }
        
        ~scope_guard() { if (engaged) f(); }
        
        void release() { engaged = false; }
    };
    
    template<typename F>
    scope_guard<uncvref<F>> make_scope_guard(F&& f) { return {(F&&)f}; }
LSTM_DETAIL_END

#endif /* LSTM_SCOPE_GUARD_HPP */
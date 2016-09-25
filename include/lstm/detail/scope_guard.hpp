#ifndef LSTM_SCOPE_GUARD_HPP
#define LSTM_SCOPE_GUARD_HPP

#include <lstm/detail/lstm_fwd.hpp>

LSTM_DETAIL_BEGIN
    template<typename Func>
    struct scope_guard {
        static_assert(noexcept(std::declval<Func&>()()), "");
    private:
        Func func;
        bool engaged;
        
    public:
        scope_guard(Func func_in) noexcept(std::is_nothrow_move_constructible<Func>{})
            : func(std::move(func_in))
            , engaged(true)
        {}
        
        scope_guard(scope_guard&& rhs) noexcept(std::is_nothrow_move_constructible<Func>{})
            : func(std::move(rhs.func))
            , engaged(rhs.engaged)
        { rhs.engaged = false; }
        
        ~scope_guard() noexcept { if (engaged) func(); }
        
        void release() noexcept { engaged = false; }
    };
    
    template<typename Func>
    scope_guard<uncvref<Func>> make_scope_guard(Func&& func)
        noexcept(noexcept(scope_guard<uncvref<Func>>{(Func&&)func}))
    { return {(Func&&)func}; }
LSTM_DETAIL_END

#endif /* LSTM_SCOPE_GUARD_HPP */
#ifndef LSTM_DETAIL_GP_CALLBACK_HPP
#define LSTM_DETAIL_GP_CALLBACK_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    constexpr std::size_t gp_callback_sbo_size = 24;
    constexpr std::size_t gp_callback_align_size = alignof(std::max_align_t);
    static_assert(sizeof(void*) <= gp_callback_sbo_size);
    static_assert(alignof(void*) <= gp_callback_sbo_size);
    
    struct gp_callback_base { virtual void operator()() const = 0; };
    
    template<typename F,
             bool = std::is_empty<F>{} && !std::is_final<F>{}, // ebo
             bool = (sizeof(F) <= gp_callback_sbo_size) && // simple sbo
                    (alignof(F) <= gp_callback_align_size) &&
                    std::is_trivially_destructible<F>{} &&
                    std::is_trivially_copy_constructible<F>{} &&
                    std::is_trivially_move_constructible<F>{}>
    struct gp_callback_impl;
    
    template<typename F>
    struct gp_callback_impl<F, true, true> : gp_callback_base, private F {
        template<typename FIn>
        gp_callback_impl(FIn&& f) noexcept(std::is_nothrow_constructible<F, FIn&&>{})
            : F((FIn&&)f)
        {}
        
        void operator()() const override final { static_cast<const F&>(*this)(); }
    };
    
    template<typename F>
    struct gp_callback_impl<F, false, true> : gp_callback_base {
    private:
        F f;
    
    public:
        template<typename FIn>
        gp_callback_impl(FIn&& f_in) noexcept(std::is_nothrow_constructible<F, FIn&&>{})
            : f((FIn&&)f_in)
        {}
        
        void operator()() const override final { f(); }
    };
    
    template<typename F, bool EBO, bool SBO>
    struct gp_callback_impl : gp_callback_base {
    private:
        F* f;
    
    public:
        template<typename FIn>
        gp_callback_impl(FIn&& f_in)
            : f(::new F((FIn&&)f_in))
        {}
        
        void operator()() const override final {
            F* f_copy = f;
            (*f_copy)();
            ::delete f_copy;
        }
    };
    
    struct gp_callback_buf {
    private:
        std::aligned_storage<gp_callback_sbo_size, gp_callback_align_size> buf;
    
    public:
        gp_callback_buf() noexcept = default;
        
        template<typename FIn>
        gp_callback_buf(FIn&& f)
            noexcept(std::is_nothrow_constructible<gp_callback_impl<uncvref<FIn>>, FIn&&>{})
        { ::new(&buf) gp_callback_impl<uncvref<FIn>>((FIn&&)f); }
        
        void operator()() const
        { (*static_cast<const gp_callback_base*>(static_cast<const void*>(&buf)))(); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_GP_CALLBACK_HPP */
#ifndef LSTM_DETAIL_GP_CALLBACK_HPP
#define LSTM_DETAIL_GP_CALLBACK_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    constexpr std::size_t gp_callback_sbo_size = sizeof(void(*)()) * 2;
    constexpr std::size_t gp_callback_align_size = alignof(std::max_align_t);
    using storage_requirements = std::aligned_storage_t<gp_callback_sbo_size,
                                                        gp_callback_align_size>;
    static_assert(sizeof(void*) <= gp_callback_sbo_size);
    static_assert(alignof(void*) <= gp_callback_sbo_size);
    
    struct gp_callback_base { virtual void operator()() noexcept = 0; };
    
    template<typename F,
             bool = (sizeof(F) <= gp_callback_sbo_size) && // simple sbo
                    (alignof(F) <= gp_callback_align_size) &&
                    std::is_trivially_destructible<F>{} &&
                    std::is_trivially_copy_constructible<F>{} &&
                    std::is_trivially_move_constructible<F>{}>
    struct gp_callback_impl;
    
    template<typename F>
    struct gp_callback_impl<F, true> : gp_callback_base {
        static constexpr const bool SBO = true;
        
    private:
        F f;
    
    public:
        template<typename FIn>
        gp_callback_impl(FIn&& f_in) noexcept(std::is_nothrow_constructible<F, FIn&&>{})
            : f((FIn&&)f_in)
        {}
    
        void operator()() noexcept override final { f(); }
    };
    
    template<typename F, bool SBO_>
    struct gp_callback_impl : gp_callback_base {
        static constexpr const bool SBO = SBO_;
        
    private:
        F* f;
    
    public:
        template<typename FIn>
        gp_callback_impl(FIn&& f_in)
            : f(::new F((FIn&&)f_in))
        {}
        
        void operator()() noexcept override final {
            F* f_copy = f;
            (*f_copy)();
            ::delete f_copy;
        }
    };
    
    struct gp_callback_buf {
    private:
        using max_buf_t = gp_callback_impl<storage_requirements>;
        static_assert(std::is_same<max_buf_t, gp_callback_impl<storage_requirements, true>>{});
        static_assert(sizeof(max_buf_t) >= sizeof(gp_callback_impl<int, false>));
        static_assert(alignof(max_buf_t) % alignof(gp_callback_impl<int, false>) == 0);
        
        uninitialized<max_buf_t> buf;
    
    public:
        gp_callback_buf() noexcept = default;
        
        template<typename FIn, typename CBType = gp_callback_impl<uncvref<FIn>>>
        gp_callback_buf(FIn&& f)
            noexcept(std::is_nothrow_constructible<CBType, FIn&&>{})
        {
            static_assert(sizeof(CBType) <= sizeof(decltype(buf)));
            static_assert(alignof(decltype(buf)) % alignof(CBType) == 0);
            static_assert(noexcept(std::declval<uncvref<FIn>&>()()));
            
            ::new(&this->buf) CBType((FIn&&)f);
        }
        
        void operator()() noexcept
        { (*static_cast<gp_callback_base*>(static_cast<void*>(&buf)))(); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_GP_CALLBACK_HPP */
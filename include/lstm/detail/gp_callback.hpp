#ifndef LSTM_DETAIL_GP_CALLBACK_HPP
#define LSTM_DETAIL_GP_CALLBACK_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    struct gp_callback {
        static constexpr auto max_payload_size = sizeof(void*) * 3;
        
        using cb_payload_t = std::aligned_storage_t<max_payload_size, alignof(std::max_align_t)>;
        using cb_t = void (*)(cb_payload_t);
        
        cb_payload_t payload;
        cb_t cb;
        
        void operator()() const { cb(payload); }
        
        gp_callback() noexcept = default;
        
        template<typename F,
            LSTM_REQUIRES_(sizeof(uncvref<F>) <= max_payload_size)>
        gp_callback(F&& f) noexcept(std::is_nothrow_copy_constructible<uncvref<F>>{}) {
            static_assert(alignof(uncvref<F>) <= alignof(cb_payload_t),
                          "gp_callback currently only supports alignof(std::max_align_t) or less "
                          "for small buffer optimizations");
            
            ::new(&payload) uncvref<F>((F&&)f);
            cb = [](cb_payload_t payload) { (*reinterpret_cast<uncvref<F>*>(&payload))(); };
        }
        
        template<typename F,
            LSTM_REQUIRES_(sizeof(uncvref<F>) > max_payload_size)>
        gp_callback(F&& f) {
            ::new(&payload) uncvref<F>*(::new uncvref<F>((F&&)f));
            cb = [](cb_payload_t payload) {
                (**reinterpret_cast<uncvref<F>**>(&payload))();
                ::delete *reinterpret_cast<uncvref<F>**>(&payload);
            };
        }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_GP_CALLBACK_HPP */
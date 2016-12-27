#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc,
        LSTM_REQUIRES_(!std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       !std::is_const<Alloc>{})>
    inline T* allocate(Alloc& alloc,
                       thread_data& tls_td = tls_thread_data())
    { return tls_td.allocate<T>(alloc); }
    
    template<typename T>
    inline T* allocate(thread_data& tls_td = tls_thread_data()) {
        static std::allocator<T> alloc{};
        return allocate(alloc, tls_td);
    }
LSTM_END

#endif /* LSTM_MEMORY_HPP */
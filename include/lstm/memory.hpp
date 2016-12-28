#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       !std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc, thread_data& tls_td = tls_thread_data())
    { return tls_td.allocate(alloc); }
LSTM_END

#endif /* LSTM_MEMORY_HPP */
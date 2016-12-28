#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc, thread_data& tls_td = tls_thread_data()) {
        Pointer result = AllocTraits::allocate(alloc, 1);
        if (tls_td.in_transaction()) {
            tls_td.queue_fail_callback([alloc = &alloc, result] {
                AllocTraits::deallocate(*alloc, result, 1);
            });
        }
        return result;
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc,
                            const std::size_t count,
                            thread_data& tls_td = tls_thread_data()) {
        Pointer result = AllocTraits::allocate(alloc, count);
        if (tls_td.in_transaction()) {
            tls_td.queue_fail_callback([alloc = &alloc, result, count] {
                AllocTraits::deallocate(*alloc, result, count);
            });
        }
        return result;
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc,
                           typename AllocTraits::pointer ptr,
                           thread_data& tls_td = tls_thread_data()) {
        if (tls_td.in_critical_section()) {
            tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr)]() mutable {
                AllocTraits::deallocate(*alloc, std::move(ptr), 1);
            });
        } else {
            AllocTraits::deallocate(alloc, std::move(ptr), 1);
        }
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc,
                           typename AllocTraits::pointer ptr,
                           const std::size_t count,
                           thread_data& tls_td = tls_thread_data()) {
        if (tls_td.in_critical_section()) {
            tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr), count]() mutable {
                AllocTraits::deallocate(*alloc, std::move(ptr), count);
            });
        } else {
            AllocTraits::deallocate(alloc, std::move(ptr), count);
        }
    }
    
    // TODO: new,
    //       delete,
    //       some kinda tx_safe_allocator
LSTM_END

#endif /* LSTM_MEMORY_HPP */
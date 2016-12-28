#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

LSTM_BEGIN
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(thread_data& tls_td, Alloc& alloc) {
        Pointer result = AllocTraits::allocate(alloc, 1);
        if (tls_td.in_critical_section()) {
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
    inline Pointer allocate(thread_data& tls_td,
                            Alloc& alloc,
                            const std::size_t count) {
        Pointer result = AllocTraits::allocate(alloc, count);
        if (tls_td.in_critical_section()) {
            tls_td.queue_fail_callback([alloc = &alloc, result, count] {
                AllocTraits::deallocate(*alloc, result, count);
            });
        }
        return result;
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc)
    { return lstm::allocate(tls_thread_data(), alloc); }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc, const std::size_t count)
    { return lstm::allocate(tls_thread_data(), alloc, count); }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(thread_data& tls_td,
                           Alloc& alloc,
                           typename AllocTraits::pointer ptr) {
        tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr)]() mutable {
            AllocTraits::deallocate(*alloc, std::move(ptr), 1);
        });
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(thread_data& tls_td,
                           Alloc& alloc,
                           typename AllocTraits::pointer ptr,
                           const std::size_t count) {
        tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr), count]() mutable {
            AllocTraits::deallocate(*alloc, std::move(ptr), count);
        });
    }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc, typename AllocTraits::pointer ptr)
    { return lstm::deallocate(tls_thread_data(), alloc, ptr); }
    
    template<typename Alloc,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc,
                           typename AllocTraits::pointer ptr,
                           const std::size_t count)
    { return lstm::deallocate(tls_thread_data(), alloc, ptr, count); }
    
    template<typename Alloc,
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_trivially_destructible<T>{})>
    inline void construct(thread_data& tls_td,
                          Alloc& alloc,
                          T* t,
                          Args&&... args) {
        AllocTraits::construct(alloc, t, (Args&&)args...);
        if (tls_td.in_critical_section()) {
            tls_td.queue_fail_callback([alloc = &alloc, t] {
                AllocTraits::destroy(*alloc, t);
            });
        }
    }
    
    template<typename Alloc,
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void construct(thread_data&,
                          Alloc& alloc,
                          T* t,
                          Args&&... args)
    { AllocTraits::construct(alloc, t, (Args&&)args...); }
    
    template<typename Alloc,
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       !std::is_trivially_destructible<T>{})>
    inline void construct(Alloc& alloc,
                          T* t,
                          Args&&... args)
    { lstm::construct(tls_thread_data(), alloc, t, (Args&&)args...); }
    
    template<typename Alloc,
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void construct(Alloc& alloc,
                          T* t,
                          Args&&... args)
    { AllocTraits::construct(alloc, t, (Args&&)args...); }
    
    template<typename Alloc,
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_trivially_destructible<T>{})>
    inline void destroy(thread_data& tls_td, Alloc& alloc, T* t) {
        tls_td.queue_succ_callback([alloc = &alloc, t] {
            AllocTraits::destroy(*alloc, t);
        });
    }
    
    template<typename Alloc,
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void destroy(thread_data&, Alloc&, T*) noexcept {}
    
    template<typename Alloc,
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       !std::is_trivially_destructible<T>{})>
    inline void destroy(Alloc& alloc, T* t)
    { lstm::destroy(tls_thread_data(), alloc, t); }
    
    template<typename Alloc,
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void destroy(Alloc&, T*) noexcept {}
    
    // TODO: new,
    //       delete,
    //       some kinda tx_safe_allocator
LSTM_END

#endif /* LSTM_MEMORY_HPP */
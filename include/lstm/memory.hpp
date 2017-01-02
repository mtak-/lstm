#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

// TODO: hacked some garbage traits stuff in here to make progress on some outstanding
// problems in the library. the correct solutions are still TBD
// it's not optimized nor pretty

LSTM_BEGIN
    namespace detail {
        template<typename Alloc, typename = void>
        struct has_value_type : std::false_type {};
        
        template<typename Alloc>
        struct has_value_type<Alloc, void_<typename Alloc::value_type>> : std::true_type {};
    }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc)
    { return lstm::allocate(tls_thread_data(), alloc); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(Alloc& alloc, const std::size_t count)
    { return lstm::allocate(tls_thread_data(), alloc, count); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc, typename AllocTraits::pointer ptr)
    { return lstm::deallocate(tls_thread_data(), alloc, ptr); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(Alloc& alloc,
                           typename AllocTraits::pointer ptr,
                           const std::size_t count)
    { return lstm::deallocate(tls_thread_data(), alloc, ptr, count); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
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
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void destroy(thread_data&, Alloc&, T*) noexcept {}
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       !std::is_trivially_destructible<T>{})>
    inline void destroy(Alloc& alloc, T* t)
    { lstm::destroy(tls_thread_data(), alloc, t); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{} &&
                       std::is_trivially_destructible<T>{})>
    inline void destroy(Alloc&, T*) noexcept {}
    
    template<typename Alloc, typename... Args,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(!std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(thread_data& tls_td, Alloc& alloc, Args&&... args) {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args&&)args...);
        if (tls_td.in_critical_section()) {
            tls_td.queue_fail_callback([alloc = &alloc, result]() mutable {
                AllocTraits::destroy(*alloc, detail::to_raw_pointer(result));
                AllocTraits::deallocate(*alloc, std::move(result), 1);
            });
        }
        return result;
    }
    
    template<typename Alloc,
             typename... Args,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer = typename AllocTraits::pointer,
        LSTM_REQUIRES_(std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(thread_data& tls_td,
                                      Alloc& alloc,
                                      Args&&... args) {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args&&)args...);
        if (tls_td.in_critical_section()) {
            tls_td.queue_fail_callback([alloc = &alloc, result] {
                AllocTraits::deallocate(*alloc, std::move(result), 1);
            });
        }
        return result;
    }
    
    template<typename Alloc,
             typename... Args,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_same<detail::uncvref<Alloc>, thread_data>{})>
    inline auto allocate_construct(Alloc& alloc, Args&&... args)
        -> decltype(lstm::allocate_construct(tls_thread_data(), alloc, (Args&&)args...))
    { return lstm::allocate_construct(tls_thread_data(), alloc, (Args&&)args...); }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       !std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline void destroy_deallocate(thread_data& tls_td,
                                   Alloc& alloc,
                                   typename AllocTraits::pointer ptr) {
        tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr)] {
            AllocTraits::destroy(*alloc, detail::to_raw_pointer(ptr));
            AllocTraits::deallocate(*alloc, std::move(ptr), 1);
        });
    }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{} &&
                       std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline void destroy_deallocate(thread_data& tls_td,
                                   Alloc& alloc,
                                   typename AllocTraits::pointer ptr) {
        tls_td.queue_succ_callback([alloc = &alloc, ptr = std::move(ptr)] {
            AllocTraits::deallocate(*alloc, std::move(ptr), 1);
        });
    }
    
    template<typename Alloc,
        LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
        LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void destroy_deallocate(Alloc& alloc, typename AllocTraits::pointer ptr)
    { lstm::destroy_deallocate(tls_thread_data(), alloc, std::move(ptr)); }
    
    namespace detail {
        template<typename Alloc, bool = std::is_empty<Alloc>{} &&
                                        !std::is_final<Alloc>{} &&
                                        std::is_trivially_destructible<Alloc>{}>
        struct tx_alloc_adaptor_base {
        private:
            Alloc _alloc;
            
        protected:
            constexpr tx_alloc_adaptor_base() = default;
            constexpr tx_alloc_adaptor_base(const Alloc& in_alloc)
                : _alloc(in_alloc)
            {}
            constexpr tx_alloc_adaptor_base(Alloc&& in_alloc)
                : _alloc(std::move(in_alloc))
            {}
            
            Alloc& alloc() { return _alloc; }
            const Alloc& alloc() const { return _alloc; }
        };
        
        template<typename Alloc>
        struct tx_alloc_adaptor_base<Alloc, true> : private Alloc {
        protected:
            constexpr tx_alloc_adaptor_base() = default;
            constexpr tx_alloc_adaptor_base(const Alloc& in_alloc)
                noexcept(std::is_nothrow_constructible<Alloc, const Alloc&>{})
                : Alloc(in_alloc)
            {}
            constexpr tx_alloc_adaptor_base(Alloc&& in_alloc)
                noexcept(std::is_nothrow_constructible<Alloc, Alloc&&>{})
                : Alloc(std::move(in_alloc))
            {}
            Alloc& alloc() { return *this; }
            const Alloc& alloc() const { return *this; }
        };
    }
    
    // this class will wrap an allocator, and reclaim memory on tx fails
    // it also queues up deallocation/destruction
    // plugging this into a std container does NOT make it tx safe
    //
    // TODO: this class is a WIP which is not used anywhere yet
    // it's usefulness is unknown at the moment. containers all need recoding to be tx safe
    // anyhow, so it's not like dropping one of these suckers in will fix anything
    template<typename Alloc>
    struct tx_alloc_adaptor;
    
    template<typename Alloc>
    struct tx_alloc_adaptor : private detail::tx_alloc_adaptor_base<Alloc> {
    private:
        using base = detail::tx_alloc_adaptor_base<Alloc>;
        using base::alloc;
        
        template<typename U> friend struct tx_alloc_adaptor;
        using alloc_traits = std::allocator_traits<Alloc>;
        
    public:
        using value_type = typename Alloc::value_type;
        using pointer = typename alloc_traits::pointer;
        using const_pointer = typename alloc_traits::const_pointer;
        using void_pointer = typename alloc_traits::void_pointer;
        using const_void_pointer = typename alloc_traits::const_void_pointer;
        using difference_type = typename alloc_traits::difference_type;
        using size_type = typename alloc_traits::size_type;
        using propagate_on_container_copy_assignment =
            typename alloc_traits::propagate_on_container_copy_assignment;
        using propagate_on_container_move_assignment =
            typename alloc_traits::propagate_on_container_move_assignment;
        using propagate_on_container_swap = typename alloc_traits::propagate_on_container_swap;
        using is_always_equal = typename alloc_traits::is_always_equal;
        template<typename U>
        using rebind = tx_alloc_adaptor<typename alloc_traits::template rebind_alloc<U>>;
        
        constexpr tx_alloc_adaptor() = default;
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<Alloc, U&&>{})>
        constexpr tx_alloc_adaptor(U&& u) noexcept(std::is_nothrow_constructible<Alloc, U&&>{})
            : base((U&&)u)
        {}
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<Alloc, const U&>{})>
        constexpr tx_alloc_adaptor(const tx_alloc_adaptor<U>& rhs)
            noexcept(std::is_nothrow_constructible<Alloc, const U&>{})
            : base(rhs.alloc())
        {}
        
        template<typename U,
            LSTM_REQUIRES_(std::is_constructible<Alloc, U&&>{})>
        constexpr tx_alloc_adaptor(tx_alloc_adaptor<U>&& rhs)
            noexcept(std::is_nothrow_constructible<Alloc, U&&>{})
            : base(std::move(rhs.alloc()))
        {}
        
        pointer allocate(size_type n)
        { return lstm::allocate(alloc(), n); }
        
        pointer allocate(size_type n, const_void_pointer cvptr)
        { return lstm::allocate(alloc(), n, cvptr); }
        
        void deallocate(pointer ptr, size_type n)
        { lstm::deallocate(alloc(), std::move(ptr), n); }
        
        template<typename T, typename... Args,
            LSTM_REQUIRES_(std::is_same<T, detail::uncvref<T>>{})>
        void construct(T* ptr, Args&&... args)
        { lstm::construct(alloc(), ptr, (Args&&)args...); }
        
        template<typename T>
        void destroy(T* ptr)
        { lstm::destroy(alloc(), ptr); }
        
        size_type max_size() const noexcept { return alloc_traits::max_size(alloc()); }
        
        tx_alloc_adaptor select_on_container_copy_construction()
            noexcept(noexcept(alloc_traits::select_on_container_copy_construction(alloc())))
        { return alloc_traits::select_on_container_copy_construction(alloc()); }
        
        template<typename U>
        bool operator==(const tx_alloc_adaptor<U>& rhs) const
            noexcept(noexcept(alloc() == rhs.alloc()))
        { return alloc() == rhs.alloc(); }
        
        template<typename U>
        bool operator!=(const tx_alloc_adaptor<U>& rhs) const
            noexcept(noexcept(alloc() != rhs.alloc()))
        { return alloc() != rhs.alloc(); }
    };
LSTM_END

#endif /* LSTM_MEMORY_HPP */

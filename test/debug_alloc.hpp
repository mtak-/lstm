#ifndef LSTM_TEST_DEBUG_ALLOC_HPP
#define LSTM_TEST_DEBUG_ALLOC_HPP

#include <atomic>
#include <memory>

template<std::nullptr_t = nullptr>
std::atomic<int> debug_live_allocations{0};

#ifndef NDEBUG
    template<typename T>
    struct debug_alloc;
    
    template<typename T>
    struct debug_alloc : private std::allocator<T> {
    private:
        std::allocator<T>& alloc() noexcept { return *this; }
        template<typename U> friend struct debug_alloc;
        volatile int x{0};
        
    public:
        using typename std::allocator<T>::value_type;
        template<typename U>
        struct rebind { using other = debug_alloc<U>; };
        
        constexpr debug_alloc() noexcept = default;
        template<typename U>
        constexpr debug_alloc(const debug_alloc<U>& rhs) noexcept : std::allocator<T>{rhs} {}
        
        T* allocate(std::size_t n) {
            ++x;
            debug_live_allocations<>.fetch_add(1, LSTM_RELAXED);
            return alloc().allocate(n);
        }
        
        void deallocate(T* t, std::size_t n) {
            --x;
            debug_live_allocations<>.fetch_sub(1, LSTM_RELAXED);
            alloc().deallocate(t, n);
        }
        
        bool operator==(const debug_alloc& rhs) const noexcept { return rhs == alloc(); }
        bool operator!=(const debug_alloc& rhs) const noexcept { return rhs != alloc(); }
    };
#else
    template<typename T>
    using debug_alloc = std::allocator<T>;
#endif /* NDEBUG */

#endif /* LSTM_TEST_DEBUG_ALLOC_HPP */

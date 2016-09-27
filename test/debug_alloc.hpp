#ifndef LSTM_TEST_DEBUG_ALLOC_HPP
#define LSTM_TEST_DEBUG_ALLOC_HPP

#include <atomic>
#include <memory>

template<std::nullptr_t = nullptr>
std::atomic<int> debug_live_allocations{0};

#ifndef NDEBUG
    template<typename T>
    struct debug_alloc : private std::allocator<T> {
    private:
        std::allocator<T>& alloc() noexcept { return *this; }
        
    public:
        using typename std::allocator<T>::value_type;
        
        T* allocate(std::size_t n) {
            ++debug_live_allocations<>;
            return alloc().allocate(n);
        }
        
        void deallocate(T* t, std::size_t n) {
            --debug_live_allocations<>;
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
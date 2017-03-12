#ifndef LSTM_RBTREE_TESTS_MALLOCATOR_HPP
#define LSTM_RBTREE_TESTS_MALLOCATOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

template<typename T>
struct mallocator
{
    using value_type = T;

    constexpr mallocator() noexcept = default;

    template<typename U, LSTM_REQUIRES_(!std::is_same<T, U>{})>
    LSTM_ALWAYS_INLINE constexpr mallocator(const mallocator<U>&) noexcept
    {
    }

    LSTM_ALWAYS_INLINE T* allocate(std::size_t n) const noexcept
    {
        return (T*)std::malloc(sizeof(T) * n);
    }

    LSTM_ALWAYS_INLINE void deallocate(T* ptr, std::size_t) const noexcept { std::free(ptr); }
};

template<typename T, typename U>
bool operator==(const mallocator<T>&, const mallocator<U>&)
{
    return true;
}
template<typename T, typename U>
bool operator!=(const mallocator<T>&, const mallocator<U>&)
{
    return true;
}

#endif /* LSTM_RBTREE_TESTS_MALLOCATOR_HPP */
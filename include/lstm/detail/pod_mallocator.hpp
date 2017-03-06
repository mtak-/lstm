#ifndef LSTM_DETAIL_POD_MALLOCATOR_HPP
#define LSTM_DETAIL_POD_MALLOCATOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cstdlib>

LSTM_DETAIL_BEGIN
    template<typename T>
    struct pod_mallocator
    {
        static_assert(std::is_pod<T>{}, "pod_mallocator only works with pod types!");
        using value_type = T;

        constexpr pod_mallocator() noexcept = default;

        template<typename U>
        constexpr pod_mallocator(const pod_mallocator<U>&) noexcept
        {
        }

        T* allocate(std::size_t n) noexcept
        {
            T* const result = (T*)std::malloc(sizeof(T) * n);
            if (!result)
                std::terminate();
            return result;
        }

        void deallocate(T* p, std::size_t) noexcept
        {
            LSTM_ASSERT(p);
            std::free(p);
        }
    };

    template<typename T, typename U>
    constexpr bool operator==(const pod_mallocator<T>&, const pod_mallocator<U>&) noexcept
    {
        return true;
    }

    template<typename T, typename U>
    constexpr bool operator!=(const pod_mallocator<T>&, const pod_mallocator<U>&) noexcept
    {
        return false;
    }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_POD_MALLOCATOR_HPP */
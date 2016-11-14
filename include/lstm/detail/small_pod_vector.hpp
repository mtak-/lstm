#ifndef LSTM_SMALL_POD_VECTOR_HPP
#define LSTM_SMALL_POD_VECTOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    template<typename T, int N = 4, typename Alloc = std::allocator<T>>
    struct small_pod_vector : private Alloc {
    private:
        static_assert(std::is_pod<T>{}, "");
        
        T buffer[N];
        T* begin_; T* end_;
        std::size_t capacity_;
        
        using alloc_traits = std::allocator_traits<Alloc>;
        Alloc& alloc() noexcept { return *this; }
        
        template<typename... Us>
        void emplace_back_slow_path(Us&&... us) {
            capacity_ = capacity_ * 2;
            auto newBegin = alloc_traits::allocate(alloc(), capacity_);
            assert(newBegin);
            if (newBegin != begin_)
                std::memcpy(newBegin, begin_, sizeof(T) * size());
            if ((capacity_ >> 1) > N)
                alloc_traits::deallocate(alloc(), begin_, capacity_ >> 1);
            end_ = newBegin + size();
            begin_ = newBegin;
            ::new (end_++) T((Us&&)us...);
        }
    
    public:
        using iterator = T*;
        using const_iterator = const T*;
        
        small_pod_vector(const Alloc& alloc = {})
            noexcept(std::is_nothrow_copy_constructible<Alloc>{})
            : Alloc(alloc)
            , begin_(buffer)
            , end_(buffer)
            , capacity_(N)
        {}
            
        small_pod_vector(const small_pod_vector&) = delete;
        small_pod_vector& operator=(const small_pod_vector&) = delete;
            
        ~small_pod_vector() noexcept
        { if (capacity() > N) alloc_traits::deallocate(alloc(), begin_, capacity_); }
            
        bool empty() const noexcept { return end_ == begin_; }
        std::size_t size() const noexcept { return end_ - begin_; }
        std::size_t capacity() const noexcept { return capacity_; }
        
        template<typename... Us>
        void emplace_back(Us&&... us) {
            if (size() + 1 <= capacity())
                ::new (end_++) T((Us&&)us...);
            else
                emplace_back_slow_path((Us&&)us...);
        }
        
        void unordered_erase(T* const ptr) noexcept
        { std::memmove(ptr, --end_, sizeof(T)); }
        
        void unordered_erase(const T* const ptr) noexcept
        { std::memmove((void*)ptr, --end_, sizeof(T)); }
            
        T* begin() noexcept { return begin_; }
        T* end() noexcept { return end_; }
        const T* begin() const noexcept { return begin_; }
        const T* end() const noexcept { return end_; }
        const T* cbegin() const noexcept { return begin_; }
        const T* cend() const noexcept { return end_; }
        
        void clear() noexcept { end_ = begin_; }
        
        T& operator[](const int i) noexcept { return begin_[i]; }
        const T& operator[](const int i) const noexcept { return begin_[i]; }
        
        T& back() noexcept { return end_[-1]; }
        const T& back() const noexcept { return end_[-1]; }
    };
    
    template<typename T> auto begin(small_pod_vector<T>& v) noexcept { return v.begin(); }
    template<typename T> auto end(small_pod_vector<T>& v) noexcept { return v.end(); }
    template<typename T> auto begin(const small_pod_vector<T>& v) noexcept { return v.begin(); }
    template<typename T> auto end(const small_pod_vector<T>& v) noexcept { return v.end(); }
    template<typename T> auto cbegin(const small_pod_vector<T>& v) noexcept { return v.cbegin(); }
    template<typename T> auto cend(const small_pod_vector<T>& v) noexcept { return v.cend(); }
LSTM_DETAIL_END

#endif /* LSTM_SMALL_POD_VECTOR_HPP */

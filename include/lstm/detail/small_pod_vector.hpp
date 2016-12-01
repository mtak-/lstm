#ifndef LSTM_DETAIL_SMALL_POD_VECTOR_HPP
#define LSTM_DETAIL_SMALL_POD_VECTOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    template<typename T, uword N = 4, typename Alloc = std::allocator<T>>
    struct small_pod_vector : private Alloc {
    private:
        static_assert(std::is_pod<T>{}, "");
        static_assert(N > 0,
            "small_pod_vector must have a buffer size greater than 0");
        
        T buffer[N];
        T* begin_; T* end_;
        uword capacity_;
        
        using alloc_traits = std::allocator_traits<Alloc>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
            "sorry, lstm only supports allocators that return raw pointers");
        
        Alloc& alloc() noexcept { return *this; }
        
        LSTM_NOINLINE void reserve_more() {
            capacity_ = capacity_ * 2;
            T* newBegin = alloc_traits::allocate(alloc(), capacity_);
            assert(newBegin);
            if (newBegin != begin_)
                std::memcpy(newBegin, begin_, sizeof(T) * size());
            if ((capacity_ >> 1) > N)
                alloc_traits::deallocate(alloc(), begin_, capacity_ >> 1);
            end_ = newBegin + size();
            begin_ = newBegin;
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
        
    #ifndef NDEBUG
        ~small_pod_vector() noexcept { assert(begin_ == buffer && end_ == buffer); }
    #endif
            
        void reset() noexcept {
            if (capacity() > N)
                alloc_traits::deallocate(alloc(), begin_, capacity_);
    #ifndef NDEBUG
            end_ = begin_ = buffer;
    #endif
        }
            
        bool empty() const noexcept { return end_ == begin_; }
        uword size() const noexcept { return end_ - begin_; }
        uword capacity() const noexcept { return capacity_; }
        
        template<typename... Us>
        void emplace_back(Us&&... us) {
            if (LSTM_UNLIKELY(size() == capacity()))
                reserve_more();
            ::new (end_++) T((Us&&)us...);
        }
        
        void unordered_erase(T* const ptr) noexcept {
            assert(ptr >= begin_ && ptr < end_);
            std::memmove(ptr, --end_, sizeof(T));
        }
        
        void unordered_erase(const T* const ptr) noexcept {
            assert(ptr >= begin_ && ptr < end_);
            std::memmove((void*)ptr, --end_, sizeof(T));
        }
        
        void set_end(T* const ptr) noexcept {
            assert(ptr >= begin_ && ptr <= end_);
            end_ = ptr;
        }
            
        iterator begin() noexcept { return begin_; }
        iterator end() noexcept { return end_; }
        const_iterator begin() const noexcept { return begin_; }
        const_iterator end() const noexcept { return end_; }
        const_iterator cbegin() const noexcept { return begin_; }
        const_iterator cend() const noexcept { return end_; }
        
        void clear() noexcept { end_ = begin_; }
        
        T& operator[](const int i) noexcept { return begin_[i]; }
        const T& operator[](const int i) const noexcept { return begin_[i]; }
        
        T& back() noexcept { return end_[-1]; }
        const T& back() const noexcept { return end_[-1]; }
    };
    
    template<typename T> typename small_pod_vector<T>::iterator
    begin(small_pod_vector<T>& v) noexcept { return v.begin(); }
    
    template<typename T> typename small_pod_vector<T>::iterator
    end(small_pod_vector<T>& v) noexcept { return v.end(); }
    
    template<typename T> typename small_pod_vector<T>::const_iterator
    begin(const small_pod_vector<T>& v) noexcept { return v.begin(); }
    
    template<typename T> typename small_pod_vector<T>::const_iterator
    end(const small_pod_vector<T>& v) noexcept { return v.end(); }
    
    template<typename T> typename small_pod_vector<T>::const_iterator
    cbegin(const small_pod_vector<T>& v) noexcept { return v.cbegin(); }
    
    template<typename T> typename small_pod_vector<T>::const_iterator
    cend(const small_pod_vector<T>& v) noexcept { return v.cend(); }
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_SMALL_POD_VECTOR_HPP */

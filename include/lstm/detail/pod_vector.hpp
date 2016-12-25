#ifndef LSTM_DETAIL_POD_VECTOR_HPP
#define LSTM_DETAIL_POD_VECTOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class never calls construct/destroy... there's no need for POD types
    // if an allocator "requires" construct/destroy to be called, it will be in for a surprise
    template<typename T, typename Alloc = std::allocator<T>>
    struct pod_vector : private Alloc {
    private:
        static_assert(std::is_pod<T>{}, "only works with POD types");
        static_assert(std::is_same<T, typename Alloc::value_type>{}, "");
        
        T* begin_; T* end_;
        uword capacity_;
        
        using alloc_traits = std::allocator_traits<Alloc>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
            "sorry, lstm currently only supports allocators that return raw pointers");
        
        inline Alloc& alloc() noexcept { return *this; }
        
        void reserve_more_slow_path(T* const new_begin) noexcept {
            std::memcpy(new_begin, begin_, sizeof(T) * size());
            alloc_traits::deallocate(alloc(), begin_, capacity_ >> 1);
            end_ = new_begin + size();
            begin_ = new_begin;
        }
        
        LSTM_NOINLINE void reserve_more()
            noexcept(noexcept(alloc_traits::allocate(alloc(), capacity_)))
        {
            capacity_ <<= 1;
            assert(capacity_ > size()); // zomg big transaction
            T* const new_begin = alloc_traits::allocate(alloc(), capacity_);
            assert(new_begin);
            if (new_begin != begin_)
                reserve_more_slow_path(new_begin);
        }
        
    public:
        using iterator = T*;
        using const_iterator = const T*;
        
        pod_vector(const Alloc& alloc = {})
            noexcept(std::is_nothrow_copy_constructible<Alloc>{})
            : Alloc(alloc)
            , begin_(alloc_traits::allocate(this->alloc(), 1))
            , end_(begin_)
            , capacity_(1)
        {}
        
        pod_vector(const pod_vector&) = delete;
        pod_vector& operator=(const pod_vector&) = delete;
        
    #ifndef NDEBUG
        // ensure reset has been called on destruction
        ~pod_vector() noexcept
        { assert(begin_ == nullptr && end_ == nullptr && capacity_ == 0); }
    #endif
        
        // in release, leaves object in an invalid state
        // aka this is the real destructor, but explicit. because... i want that
        void reset() noexcept {
            alloc_traits::deallocate(alloc(), begin_, capacity_);
    #ifndef NDEBUG
            end_ = begin_ = nullptr;
            capacity_ = 0;
    #endif
        }
        
        bool empty() const noexcept { return end_ == begin_; }
        uword size() const noexcept { return end_ - begin_; }
        uword capacity() const noexcept { return capacity_; }
        
        template<typename... Us>
        void emplace_back(const Us... us) noexcept(noexcept(reserve_more())) {
            static_assert(and_<std::is_pod<Us>...>{}, "");
            
            if (LSTM_UNLIKELY(size() == capacity_))
                reserve_more();
            ::new (end_++) T(us...);
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
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_POD_VECTOR_HPP */
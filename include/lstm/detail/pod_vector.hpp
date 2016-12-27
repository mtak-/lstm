#ifndef LSTM_DETAIL_POD_VECTOR_HPP
#define LSTM_DETAIL_POD_VECTOR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <cassert>

LSTM_DETAIL_BEGIN
    // this class never calls construct/destroy... there's no need for POD types
    // if an allocator "requires" construct/destroy to be called, it will be in for a surprise
    template<typename T, typename Alloc = std::allocator<T>>
    struct pod_vector : private Alloc {
        using allocator_type = Alloc;
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        
    private:
        static_assert(std::is_pod<value_type>{}, "only works with POD types");
        static_assert(std::is_same<value_type, typename allocator_type::value_type>{}, "");
        
        iterator begin_;
        iterator end_;
        uword capacity_;
        
        using alloc_traits = std::allocator_traits<allocator_type>;
        static_assert(std::is_pointer<typename alloc_traits::pointer>{},
            "sorry, lstm currently only supports allocators that return raw pointers");
        
        inline allocator_type& alloc() noexcept { return *this; }
        
        void reserve_more_slow_path(const pointer new_begin) noexcept {
            std::memcpy(new_begin, begin_, sizeof(value_type) * size());
            alloc_traits::deallocate(alloc(), begin_, capacity_ >> 1);
            end_ = new_begin + size();
            begin_ = new_begin;
        }
        
        LSTM_NOINLINE void reserve_more()
            noexcept(noexcept(alloc_traits::allocate(alloc(), capacity_)))
        {
            capacity_ <<= 1;
            assert(capacity_ > size()); // zomg big transaction
            const pointer new_begin = alloc_traits::allocate(alloc(), capacity_);
            assert(new_begin);
            if (new_begin != begin_)
                reserve_more_slow_path(new_begin);
        }
        
    public:
        pod_vector(const allocator_type& alloc = {})
            noexcept(std::is_nothrow_copy_constructible<allocator_type>{})
            : allocator_type(alloc)
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
            static_assert(and_<std::integral_constant<bool, !std::is_polymorphic<Us>{}>...>{}, "");
            static_assert(and_<std::is_standard_layout<Us>...>{}, "");
            static_assert(and_<std::is_trivially_copy_constructible<Us>...>{}, "");
            static_assert(and_<std::is_trivially_move_constructible<Us>...>{}, "");
            static_assert(and_<std::is_trivially_destructible<Us>...>{}, "");
            
            if (LSTM_UNLIKELY(size() == capacity_))
                reserve_more();
            ::new (end_++) value_type(us...);
        }
        
        void unordered_erase(const pointer ptr) noexcept {
            assert(ptr >= begin_ && ptr < end_);
            std::memmove(ptr, --end_, sizeof(value_type));
        }
        
        void unordered_erase(const const_pointer ptr) noexcept {
            assert(ptr >= begin_ && ptr < end_);
            std::memmove((void*)ptr, --end_, sizeof(value_type));
        }
        
        void set_end(const pointer ptr) noexcept {
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
        
        reference operator[](const int i) noexcept { return begin_[i]; }
        const_reference operator[](const int i) const noexcept { return begin_[i]; }
        
        reference back() noexcept { return end_[-1]; }
        const_reference back() const noexcept { return end_[-1]; }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_POD_VECTOR_HPP */
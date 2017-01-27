#ifndef LSTM_DETAIL_POD_HASH_SET_HPP
#define LSTM_DETAIL_POD_HASH_SET_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <algorithm>
#include <cmath>

LSTM_DETAIL_BEGIN
    using hash_t = std::uint64_t;
    
    template<typename T>
    static constexpr hash_t calcShift() noexcept {
        hash_t l = 0;
        hash_t foo = alignof(T);
        while (foo > 0) {
            foo >>= 1;
            ++l;
        }
        return l - 1;
    }
    
    // dumb hash need to stress test it
    // constexpr std::uint64_t prime = 11400714819323198393ull;
    inline hash_t dumb_pointer_hash(const var_base& value) noexcept {
        constexpr hash_t shift = calcShift<var_base>();
        static_assert(sizeof(hash_t) >= sizeof(std::uintptr_t) - shift,
                      "type for hash_t is not large enough");
        constexpr hash_t one{1};
        const auto raw_hash = (reinterpret_cast<std::uintptr_t>(&value) >> shift);
        return (one << (raw_hash & 63));
    }
    
    template<typename T>
    inline T* slow_find(T* begin, const uncvref<T>* const end, const var_base& value) noexcept {
        for (; begin != end; ++begin)
            if (&begin->dest_var() == &value)
                break;
        return begin;
    }
    
    // TODO: this class is only designed to work with write_set_value_type
    template<typename Underlying>
    struct pod_hash_set {
        using data_t = Underlying;
        using allocator_type = typename data_t::allocator_type;
        using value_type = typename data_t::value_type;
        using reference = typename data_t::reference;
        using const_reference = typename data_t::const_reference;
        using pointer = typename data_t::pointer;
        using const_pointer = typename data_t::const_pointer;
        using iterator = typename data_t::iterator;
        using const_iterator = typename data_t::const_iterator;
        
    private:
        hash_t filter_;
        data_t data;
        
    public:
        pod_hash_set(const allocator_type& alloc = {})
            noexcept(std::is_nothrow_copy_constructible<allocator_type>{})
            : filter_(0)
            , data(alloc)
        {}
            
        pod_hash_set(const pod_hash_set&) = delete;
        pod_hash_set& operator=(const pod_hash_set&) = delete;
        
        hash_t filter() const noexcept { return filter_; }
        
        void clear() noexcept { filter_ = 0; data.clear(); }
        void reset() noexcept { filter_ = 0; data.reset(); }
            
        bool empty() const noexcept { return data.empty(); }
        uword size() const noexcept { return data.size(); }
        uword capacity() const noexcept { return data.capacity(); }
        
        // TODO: this is the reason it only works with write_set_value_type
        // if more instantiations of this class are needed, probly just put hash
        // first and make it a template
        void push_back(var_base* const value,
                       const var_storage pending_write,
                       const hash_t hash) {
            assert(hash != 0);
            filter_ |= hash;
            data.emplace_back(value, pending_write);
        }
        
        void unordered_erase(const pointer ptr) noexcept
        { data.unordered_erase(ptr); }
        
        void unordered_erase(const const_pointer ptr) noexcept
        { data.unordered_erase(ptr); }
            
        iterator begin() noexcept { return data.begin(); }
        iterator end() noexcept { return data.end(); }
        const_iterator begin() const noexcept { return data.begin(); }
        const_iterator end() const noexcept { return data.end(); }
        const_iterator cbegin() const noexcept { return data.cbegin(); }
        const_iterator cend() const noexcept { return data.cend(); }
        
        reference operator[](const int i) noexcept { return data[i]; }
        const_reference operator[](const int i) const noexcept { return data[i]; }
        
        reference back() noexcept { return data.back(); }
        const_reference back() const noexcept { return data.back(); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_POD_HASH_SET_HPP */
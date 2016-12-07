#ifndef LSTM_DETAIL_READ_SET_HPP
#define LSTM_DETAIL_READ_SET_HPP

#include <lstm/detail/small_pod_vector.hpp>

#include <algorithm>
#include <cmath>

LSTM_DETAIL_BEGIN
    template<typename T>
    static constexpr std::uint64_t calcShift() noexcept {
        std::uint64_t l = 0;
        std::uint64_t foo = alignof(T);
        while (foo > 0) {
            foo >>= 1;
            ++l;
        }
        return l - 1;
    }
    
    // dumb hash need to stress test it
    // constexpr std::uint64_t prime = 11400714819323198393ull;
    inline std::uint64_t hash(const var_base& value) noexcept {
        static_assert(sizeof(&value) <= sizeof(std::uint64_t));
        constexpr std::uint64_t shift = calcShift<var_base>();
        constexpr std::uint64_t one{1};
        const auto raw_hash = (std::uint64_t(&value) >> shift);
        return (one << (raw_hash % 64));
    }
    
    template<typename T>
    LSTM_NOINLINE inline T* slow_find(T* begin,
                                      const T* const end,
                                      const var_base& value) noexcept {
        for (; begin != end; ++begin)
            if (&begin->dest_var() == &value)
                break;
        return begin;
    }
    
    // TODO: this class is only designed to work with write_set_value_type
    template<typename T,
             uword N,
             typename Alloc = std::allocator<T>>
    struct small_pod_hash_set {
    private:
        using data_t = small_pod_vector<T, N, Alloc>;
        std::uint64_t filter_;
        data_t data;
        
    public:
        using iterator = typename data_t::iterator;
        using const_iterator = typename data_t::const_iterator;
        static_assert(std::is_same<iterator, T*>{});
        
        small_pod_hash_set(const Alloc& alloc = {})
            noexcept(std::is_nothrow_copy_constructible<Alloc>{})
            : filter_(0)
            , data(alloc)
        {}
            
        small_pod_hash_set(const small_pod_hash_set&) = delete;
        small_pod_hash_set& operator=(const small_pod_hash_set&) = delete;
        
        std::uint64_t filter() const noexcept { return filter_; }
        
        void clear() noexcept { filter_ = 0; data.clear(); }
        void reset() noexcept { filter_ = 0; data.reset(); }
            
        bool empty() const noexcept { return data.empty(); }
        uword size() const noexcept { return data.size(); }
        uword capacity() const noexcept { return data.capacity(); }
        
        // TODO: this is the reason it only works with write_set_value_type
        // if more instantiations of this class are needed, probly just put hash
        // first and make it a template (could cause registers to be swapped)
        void push_back(var_base* const value,
                       const var_storage pending_write,
                       const std::uint64_t hash) {
            data.emplace_back(value, pending_write);
            filter_ |= hash;
        }
        
        void unordered_erase(T* const ptr) noexcept
        { data.unordered_erase(ptr); }
        
        void unordered_erase(const T* const ptr) noexcept
        { data.unordered_erase(ptr); }
            
        iterator begin() noexcept { return data.begin(); }
        iterator end() noexcept { return data.end(); }
        const_iterator begin() const noexcept { return data.begin(); }
        const_iterator end() const noexcept { return data.end(); }
        const_iterator cbegin() const noexcept { return data.cbegin(); }
        const_iterator cend() const noexcept { return data.cend(); }
        
        T& operator[](const int i) noexcept { return data[i]; }
        const T& operator[](const int i) const noexcept { return data[i]; }
        
        T& back() noexcept { return data.back(); }
        const T& back() const noexcept { return data.back(); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_READ_SET_HPP */
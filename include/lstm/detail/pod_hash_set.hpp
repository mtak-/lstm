#ifndef LSTM_DETAIL_POD_HASH_SET_HPP
#define LSTM_DETAIL_POD_HASH_SET_HPP

#include <lstm/detail/write_set_lookup.hpp>

#include <algorithm>
#include <cmath>

LSTM_DETAIL_BEGIN
    template<typename T>
    static constexpr hash_t calcShift() noexcept
    {
        hash_t l   = 0;
        hash_t foo = alignof(T);
        while (foo > 0) {
            foo >>= 1;
            ++l;
        }
        return l - 1;
    }

    template<typename T>
    inline hash_t dumb_pointer_hash(const T& value) noexcept
    {
        constexpr hash_t shift = calcShift<T>();
        static_assert(sizeof(hash_t) >= sizeof(std::uintptr_t) - shift,
                      "type for hash_t is not large enough");
        constexpr hash_t one{1};
        const auto       raw_hash = (reinterpret_cast<std::uintptr_t>(&value) >> shift);
        return (one << (raw_hash & 63));
    }

    // TODO: this class is only designed to work with write_set_value_type
    template<typename Underlying>
    struct pod_hash_set
    {
        using data_t          = Underlying;
        using allocator_type  = typename data_t::allocator_type;
        using value_type      = typename data_t::value_type;
        using reference       = typename data_t::reference;
        using const_reference = typename data_t::const_reference;
        using pointer         = typename data_t::pointer;
        using const_pointer   = typename data_t::const_pointer;
        using iterator        = typename data_t::iterator;
        using const_iterator  = typename data_t::const_iterator;

    private:
        hash_t filter_;
        data_t data;

        iterator find_impl(const var_base& value) noexcept
        {
            iterator              result = begin();
            const var_base* const ptr    = &value;
            while (result != end() && ptr != &result->dest_var())
                ++result;
            return result;
        }

        LSTM_NOINLINE const_iterator find_slow_path(const var_base& dest_var) const noexcept
        {
            const const_iterator iter = const_cast<pod_hash_set&>(*this).find_impl(dest_var);
            if (iter != end()) {
                LSTM_LOG_BLOOM_SUCCESS();

                return iter;
            } else {
                LSTM_LOG_BLOOM_COLLISION();

                return iter;
            }
        }

        LSTM_NOINLINE write_set_lookup lookup_slow_path(const var_base& dest_var,
                                                        const hash_t    hash) noexcept
        {
            const iterator iter = find_impl(dest_var);
            if (iter != end()) {
                LSTM_LOG_BLOOM_SUCCESS();

                return write_set_lookup{&iter->pending_write()};
            } else {
                LSTM_LOG_BLOOM_COLLISION();

                return write_set_lookup{hash};
            }
        }

    public:
        pod_hash_set(const allocator_type& alloc = {}) noexcept
            : filter_(0)
            , data(alloc)
        {
        }

        pod_hash_set(const pod_hash_set&) = delete;
        pod_hash_set& operator=(const pod_hash_set&) = delete;

        hash_t filter() const noexcept { return filter_; }

        void clear() noexcept
        {
            filter_ = 0;
            data.clear();
        }

        bool  empty() const noexcept { return data.empty(); }
        uword size() const noexcept { return data.size(); }
        uword capacity() const noexcept { return data.capacity(); }

        void push_back(var_base* const value, const var_storage pending_write, const hash_t hash)
        {
            assert(hash != 0);
            filter_ |= hash;
            data.emplace_back(value, pending_write);
        }

        // biased against finding the var
        const_iterator find(const var_base& dest_var) const noexcept
        {
            if (LSTM_LIKELY(!(filter_ & dumb_pointer_hash(dest_var)))) {
                LSTM_LOG_BLOOM_SUCCESS();

                return end();
            }
            return find_slow_path(dest_var);
        }

        // biased against finding the var
        write_set_lookup lookup(const var_base& dest_var) noexcept
        {
            const hash_t hash = dumb_pointer_hash(dest_var);
            if (LSTM_LIKELY(!(filter_ & hash))) {
                LSTM_LOG_BLOOM_SUCCESS();

                return write_set_lookup{hash};
            }
            return lookup_slow_path(dest_var, hash);
        }

        iterator       begin() noexcept { return data.begin(); }
        iterator       end() noexcept { return data.end(); }
        const_iterator begin() const noexcept { return data.begin(); }
        const_iterator end() const noexcept { return data.end(); }
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_POD_HASH_SET_HPP */
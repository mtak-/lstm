#ifndef LSTM_EASY_VAR_HPP
#define LSTM_EASY_VAR_HPP

#include <lstm/detail/easy_var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct easy_var : detail::easy_var_impl<T, Alloc> {
    private:
        using base = detail::easy_var_impl<T, Alloc>;
        
    public:
        using detail::easy_var_impl<T, Alloc>::easy_var_impl;
        
        easy_var(const easy_var& rhs) : base(rhs.get()) {}
        
        using base::underlying;
        
        easy_var& operator=(const easy_var& rhs) {
            lstm::read_write([&](auto& tx) { tx.write(underlying(), tx.read(rhs.underlying())); });
            return *this;
        }
        
        template<typename U = T,
            LSTM_REQUIRES_(std::is_assignable<T&, const U&>())>
        easy_var& operator=(const U& rhs) {
            lstm::read_write([&](auto& tx) { tx.write(underlying(), rhs); });
            return *this;
        }
        
        T get() const
        { return lstm::read_write([this](auto& tx) { return tx.read(underlying()); }); }
        
        inline operator T() const { return get(); }
        
        T unsafe_read() const noexcept { return underlying().unsafe_read(); }
        void unsafe_write(const T& t) noexcept { return underlying().unsafe_write(t); }
        void unsafe_write(T&& t) noexcept { return underlying().unsafe_write(std::move(t)); }
    };
LSTM_END

#endif /* LSTM_EASY_VAR_HPP */
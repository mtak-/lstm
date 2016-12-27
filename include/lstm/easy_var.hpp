#ifndef LSTM_EASY_VAR_HPP
#define LSTM_EASY_VAR_HPP

#include <lstm/detail/easy_var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct easy_var : detail::easy_var_impl<T, Alloc> {
    private:
        using detail::easy_var_impl<T, Alloc>::var;
        using base = detail::easy_var_impl<T, Alloc>;
        
    public:
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<base, Us&&...>())>
        inline easy_var(Us&&... us) noexcept(std::is_nothrow_constructible<base, Us&&...>{})
            : base((Us&&)us...)
        {}
        
        easy_var(const easy_var& rhs) : base(rhs.get()) {}
        
        easy_var& operator=(const easy_var& rhs) {
            lstm::read_write([&](auto& tx) { tx.write(var, tx.read(rhs.var)); });
            return *this;
        }
        
        template<typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, const U&>())>
        easy_var& operator=(const U& rhs) {
            lstm::read_write([&](auto& tx) { tx.write(var, rhs); });
            return *this;
        }
        
        T get() const { return lstm::read_write([this](auto& tx) { return tx.read(var); }); }
        
        inline operator T() const { return get(); }
        
        T unsafe_read() const noexcept { return var.unsafe_read(); }
        void unsafe_write(const T& t) noexcept { return var.unsafe_write(t); }
        void unsafe_write(T&& t) noexcept { return var.unsafe_write(std::move(t)); }
    };
LSTM_END

#endif /* LSTM_EASY_VAR_HPP */
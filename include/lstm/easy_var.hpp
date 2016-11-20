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
            lstm::atomic([&](auto& tx) { tx.store(var, tx.load(rhs.var)); });
            return *this;
        }
        
        template<typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, const U&>())>
        easy_var& operator=(const U& rhs) {
            lstm::atomic([&](auto& tx) { tx.store(var, rhs); });
            return *this;
        }
        
        T get() const { return lstm::atomic([this](auto& tx) { return tx.load(var); }); }
        
        inline operator T() const { return get(); }
        
        T unsafe_load() const noexcept { return var.unsafe_load(); }
        void unsafe_store(const T& t) noexcept { return var.unsafe_store(t); }
        void unsafe_store(T&& t) noexcept { return var.unsafe_store(std::move(t)); }
    };
LSTM_END

#endif /* LSTM_EASY_VAR_HPP */
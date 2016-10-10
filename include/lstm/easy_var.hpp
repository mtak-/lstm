#ifndef LSTM_EASY_VAR_HPP
#define LSTM_EASY_VAR_HPP

#include <lstm/lstm.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc = std::allocator<T>>
    struct easy_var
    {
    private:
        using var_t = var<T, Alloc>;
        var_t var;
        
    public:
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<var_t, Us&&...>())>
        easy_var(Us&&... us) noexcept(std::is_nothrow_constructible<var_t, Us&&...>{})
            : var((Us&&)us...)
        {}
        
        easy_var(const easy_var& rhs)
            : var(rhs.get())
        {}
        
        easy_var& operator=(const easy_var& rhs) {
            lstm::atomic([&](auto& tx) {
                tx.store(var, tx.load(rhs.var));
            });
            return *this;
        }
        
        template<typename U,
            LSTM_REQUIRES_(std::is_assignable<T&, const U&>())>
        easy_var& operator=(const U& rhs) {
            lstm::atomic([&](auto& tx) {
                tx.store(var, rhs);
            });
            return *this;
        }
        
        T get() const {
            return lstm::atomic([this](auto& tx) {
                return tx.load(var);
            });
        }
        
        operator T() const { return get(); }
        
        T& unsafe() & noexcept { return var.unsafe(); }
        T&& unsafe() && noexcept { return std::move(var.unsafe()); }
        const T& unsafe() const & noexcept { return var.unsafe(); }
        const T&& unsafe() const && noexcept { return std::move(var.unsafe()); }
    };
LSTM_END

#endif /* LSTM_EASY_VAR_HPP */
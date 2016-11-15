#ifndef LSTM_VAR_HPP
#define LSTM_VAR_HPP

#include <lstm/detail/var_detail.hpp>

LSTM_BEGIN
    template<typename T, typename Alloc>
    struct var
        : detail::var_alloc_policy<T, Alloc>
    {
    private:
        using base = detail::var_alloc_policy<T, Alloc>;
        static_assert(!std::is_reference<T>{},
            "a var cannot contain a reference");
        static_assert(std::is_same<std::remove_reference_t<T>, typename Alloc::value_type>{},
            "invalid allocator for type T");
        static_assert(!std::is_array<T>{},
            "raw c arrays are not allowed. try using a std::array");
        
    public:
        template<typename U, typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, U&&, Us&&...>() &&
                           !std::is_same<detail::uncvref<U>, detail::uncvref<Alloc>>())>
        constexpr var(U&& u, Us&&... us)
            noexcept(noexcept(base::allocate_construct((U&&)u, (Us&&)us...)))
        { detail::var_base::storage = base::allocate_construct((U&&)u, (Us&&)us...); }
        
        LSTM_REQUIRES(std::is_constructible<T>())
        constexpr var() noexcept(noexcept(base::allocate_construct()))
        { detail::var_base::storage = base::allocate_construct(); }
        
        template<typename... Us,
            LSTM_REQUIRES_(std::is_constructible<T, Us&&...>())>
        constexpr var(const Alloc& in_alloc, Us&&... us)
            noexcept(noexcept(base::allocate_construct((Us&&)us...)))
            : base{in_alloc}
        { detail::var_base::storage = base::allocate_construct((Us&&)us...); }
        
        ~var() noexcept override final { base::destroy_deallocate(detail::var_base::storage); }
        
        // TODO: return types are wrong for atomic types        
        decltype(auto) unsafe_load() const & noexcept
        { return base::load(detail::var_base::storage.load(LSTM_RELAXED)); }
        
        decltype(auto) unsafe_load() const && noexcept
        { return std::move(base::load(detail::var_base::storage.load(LSTM_RELAXED))); }
        
        // TODO: return types are wrong for atomic types
        void unsafe_store(const T& t) noexcept
        { return base::store(t); }
        
        void unsafe_store(T&& t) noexcept
        { return base::store(std::move(t)); }
    };
LSTM_END

#endif /* LSTM_VAR_HPP */

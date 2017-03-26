#ifndef LSTM_DETAIL_VAR_HPP
#define LSTM_DETAIL_VAR_HPP

#include <lstm/detail/lstm_fwd.hpp>

#include <atomic>

LSTM_BEGIN
    enum class var_type
    {
        heap,
        atomic,
    };
LSTM_END

LSTM_DETAIL_BEGIN
    struct var_aligner
    {
        std::atomic<epoch_t>     _dummy0;
        std::atomic<var_storage> _dummy1;
    };

    struct alignas(alignof(var_aligner) << 1) var_base
    {
    protected:
        std::atomic<epoch_t>     version_lock;
        std::atomic<var_storage> storage;

        explicit var_base(const var_storage in_storage) noexcept
            : version_lock{0}
            , storage{in_storage}
        {
        }

        friend struct ::lstm::detail::transaction_base;
        friend commit_algorithm;
    };

    template<typename T>
    constexpr var_type var_type_switch() noexcept
    {
        if (sizeof(T) <= sizeof(var_storage) && alignof(T) <= alignof(var_storage)
            && std::is_trivially_copy_constructible<T>{}()
            && std::is_trivially_move_constructible<T>{}()
            && std::is_trivially_copy_assignable<T>{}()
            && std::is_trivially_move_assignable<T>{}()
            && std::is_trivially_destructible<T>{}())
            return var_type::atomic;
        return var_type::heap;
    }

    template<typename Alloc, bool = std::is_final<Alloc>{}>
    struct alloc_wrapper
    {
    private:
        Alloc alloc_;

    public:
        alloc_wrapper() = default;
        alloc_wrapper(const Alloc& in_alloc) noexcept
            : alloc_(in_alloc)
        {
        }

        Alloc&       alloc() noexcept { return alloc_; }
        const Alloc& alloc() const noexcept { return alloc_; }
    };

    template<typename Alloc>
    struct alloc_wrapper<Alloc, false> : private Alloc
    {
        alloc_wrapper() = default;
        alloc_wrapper(const Alloc& in_alloc) noexcept
            : Alloc(in_alloc)
        {
        }

        Alloc&       alloc() noexcept { return *this; }
        const Alloc& alloc() const noexcept { return *this; }
    };

    template<typename T, typename Alloc, var_type Var_type = var_type_switch<T>()>
    struct var_alloc_policy : private alloc_wrapper<Alloc>, var_base
    {
        static constexpr bool     heap   = true;
        static constexpr bool     atomic = false;
        static constexpr var_type type   = Var_type;

    protected:
        using alloc_traits = std::allocator_traits<Alloc>;
        using alloc_wrapper<Alloc>::alloc;

        explicit var_alloc_policy() noexcept(noexcept(allocate_construct())
                                             && std::is_nothrow_default_constructible<Alloc>{})
            : var_base(allocate_construct())
        {
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(!std::is_same<uncvref<U>, std::allocator_arg_t>{}
                                && !std::is_same<uncvref<U>, var_alloc_policy>{})>
        explicit var_alloc_policy(U&& u, Us&&... us) noexcept(
            noexcept(allocate_construct((U &&) u, (Us &&) us...))
            && std::is_nothrow_default_constructible<Alloc>{})
            : var_base(allocate_construct((U &&) u, (Us &&) us...))
        {
        }

        template<typename... Us>
        explicit var_alloc_policy(std::allocator_arg_t,
                                  const Alloc& in_alloc,
                                  Us&&... us) noexcept(noexcept(allocate_construct((Us &&) us...)))
            : alloc_wrapper<Alloc>(in_alloc)
            , var_base(allocate_construct((Us &&) us...))
        {
        }

        ~var_alloc_policy() noexcept
        {
            var_alloc_policy::destroy_deallocate(alloc(), storage.load(LSTM_RELAXED));
        }

        template<typename... Us>
        var_storage allocate_construct(Us&&... us) noexcept(
            noexcept(alloc_traits::allocate(alloc(), 1))
            && noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us &&) us...)))
        {
            T* ptr = alloc_traits::allocate(alloc(), 1);
            if (noexcept(alloc_traits::construct(alloc(), (T*)nullptr, (Us &&) us...))) {
                alloc_traits::construct(alloc(), ptr, (Us &&) us...);
            } else {
                try {
                    alloc_traits::construct(alloc(), ptr, (Us &&) us...);
                } catch (...) {
                    var_alloc_policy::destroy_deallocate(alloc(), {ptr});
                    throw;
                }
            }
            return {ptr};
        }

        static void destroy_deallocate(Alloc& alloc, var_storage s) noexcept
        {
            T* ptr = std::addressof(load(s));
            alloc_traits::destroy(alloc, ptr);
            alloc_traits::deallocate(alloc, ptr, 1);
        }

        static T& load(var_storage storage) noexcept { return *static_cast<T*>(storage.ptr); }

        template<typename U>
        static void
        store(var_storage storage, U&& u) noexcept(std::is_nothrow_assignable<T&, U&&>{})
        {
            load(storage) = (U &&) u;
        }

        template<typename U>
        static void store(std::atomic<var_storage>& storage,
                          U&& u) noexcept(std::is_nothrow_assignable<T&, U&&>{})
        {
            store(storage.load(LSTM_RELAXED), (U &&) u);
        }

        template<typename U>
        static void store(const std::atomic<var_storage>& storage, U&&) noexcept = delete;
        template<typename U>
        static void store(const std::atomic<var_storage>&& storage, U&&) noexcept = delete;
    };

    template<typename T, typename Alloc>
    struct var_alloc_policy<T, Alloc, var_type::atomic> : private alloc_wrapper<Alloc>, var_base
    {
        static constexpr bool     heap   = false;
        static constexpr bool     atomic = true;
        static constexpr var_type type   = var_type::atomic;

    protected:
        using alloc_traits = std::allocator_traits<Alloc>;
        using alloc_wrapper<Alloc>::alloc;

        explicit var_alloc_policy() noexcept(noexcept(allocate_construct())
                                             && std::is_nothrow_default_constructible<Alloc>{})
            : var_base(allocate_construct())
        {
        }

        template<typename U,
                 typename... Us,
                 LSTM_REQUIRES_(!std::is_same<uncvref<U>, std::allocator_arg_t>{}
                                && !std::is_same<uncvref<U>, var_alloc_policy>{})>
        explicit var_alloc_policy(U&& u, Us&&... us) noexcept(
            noexcept(allocate_construct((U &&) u, (Us &&) us...))
            && std::is_nothrow_default_constructible<Alloc>{})
            : var_base(allocate_construct((U &&) u, (Us &&) us...))
        {
        }

        template<typename... Us>
        explicit var_alloc_policy(std::allocator_arg_t,
                                  const Alloc& in_alloc,
                                  Us&&... us) noexcept(noexcept(allocate_construct((Us &&) us...)))
            : alloc_wrapper<Alloc>(in_alloc)
            , var_base(allocate_construct((Us &&) us...))
        {
        }

        template<typename... Us>
        var_storage
        allocate_construct(Us&&... us) noexcept(std::is_nothrow_constructible<T, Us&&...>{})
        {
            var_storage result;
            alloc_traits::construct(alloc(), reinterpret_cast<T*>(result.raw), (Us &&) us...);
            return result;
        }

        static T load(var_storage storage) noexcept { return *reinterpret_cast<T*>(storage.raw); }

        template<typename U>
        static void store(var_storage& storage, U&& u) noexcept
        {
            *reinterpret_cast<T*>(storage.raw) = (U &&) u;
        }

        template<typename U>
        static void store(std::atomic<var_storage>& storage, U&& u) noexcept
        {
            var_storage new_storage;
            ::new (new_storage.raw) T((U &&) u);
            storage.store(new_storage, LSTM_RELAXED);
        }

        template<typename U>
        static void store(const std::atomic<var_storage>& storage, U&&) noexcept = delete;
        template<typename U>
        static void store(const std::atomic<var_storage>&& storage, U&&) noexcept = delete;
    };
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_VAR_HPP */

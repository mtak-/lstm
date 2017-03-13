#ifndef LSTM_MEMORY_HPP
#define LSTM_MEMORY_HPP

#include <lstm/thread_data.hpp>

// TODO: hacked some garbage traits stuff in here to make progress on some outstanding
// problems in the library. the correct solutions are still TBD
// it's not optimized nor pretty

LSTM_DETAIL_BEGIN
    template<typename Alloc>
    using has_value_type_ = typename Alloc::value_type;

    template<typename Alloc>
    using has_value_type = supports<has_value_type_, Alloc>;

    template<typename Tx>
    using is_transaction = std::integral_constant<bool,
                                                  std::is_same<Tx, transaction>{}
                                                      || std::is_same<Tx, read_transaction>{}>;
LSTM_DETAIL_END

LSTM_BEGIN
    template<typename Alloc,
             LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(thread_data & tls_td, Alloc & alloc)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        if (tls_td.in_critical_section()) {
            tls_td.after_fail([ alloc, result ]() mutable noexcept {
                AllocTraits::deallocate(alloc, result, 1);
            });
        }
        return result;
    }

    template<typename Alloc,
             LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(thread_data & tls_td, Alloc & alloc, const std::size_t count)
    {
        Pointer result = AllocTraits::allocate(alloc, count);
        if (tls_td.in_critical_section()) {
            tls_td.after_fail([ alloc, result, count ]() mutable noexcept {
                AllocTraits::deallocate(alloc, result, count);
            });
        }
        return result;
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(const Tx tx, Alloc& alloc)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        tx.after_fail(
            [ alloc, result ]() mutable noexcept { AllocTraits::deallocate(alloc, result, 1); });
        return result;
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline Pointer allocate(const Tx tx, Alloc& alloc, const std::size_t count)
    {
        Pointer result = AllocTraits::allocate(alloc, count);
        tx.after_fail([ alloc, result, count ]() mutable noexcept {
            AllocTraits::deallocate(alloc, result, count);
        });
        return result;
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(const Tx tx, Alloc& alloc, typename AllocTraits::pointer ptr)
    {
        tx.sometime_synchronized_after([ alloc, ptr = std::move(ptr) ]() mutable noexcept {
            AllocTraits::deallocate(alloc, std::move(ptr), 1);
        });
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{})>
    inline void deallocate(const Tx                      tx,
                           Alloc&                        alloc,
                           typename AllocTraits::pointer ptr,
                           const std::size_t             count)
    {
        tx.sometime_synchronized_after([ alloc, ptr = std::move(ptr), count ]() mutable noexcept {
            AllocTraits::deallocate(alloc, std::move(ptr), count);
        });
    }

    template<typename Alloc,
             LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && !std::is_trivially_destructible<T>{})>
    inline void construct(thread_data & tls_td, Alloc & alloc, T * t, Args && ... args)
    {
        AllocTraits::construct(alloc, t, (Args &&) args...);
        if (tls_td.in_critical_section()) {
            tls_td.after_fail([ alloc, t ]() mutable noexcept { AllocTraits::destroy(alloc, t); });
        }
    }

    template<typename Alloc,
             LSTM_REQUIRES_(detail::has_value_type<Alloc>{}),
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && std::is_trivially_destructible<T>{})>
    inline void construct(thread_data&, Alloc & alloc, T * t, Args && ... args)
    {
        AllocTraits::construct(alloc, t, (Args &&) args...);
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && !std::is_trivially_destructible<T>{})>
    inline void construct(const Tx tx, Alloc& alloc, T* t, Args&&... args)
    {
        AllocTraits::construct(alloc, t, (Args &&) args...);
        tx.after_fail([ alloc, t ]() mutable noexcept { AllocTraits::destroy(alloc, t); });
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename T,
             typename... Args,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && std::is_trivially_destructible<T>{})>
    inline void construct(const Tx, Alloc& alloc, T* t, Args&&... args)
    {
        AllocTraits::construct(alloc, t, (Args &&) args...);
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && !std::is_trivially_destructible<T>{})>
    inline void destroy(const Tx tx, Alloc& alloc, T* t)
    {
        tx.sometime_synchronized_after(
            [ alloc, t ]() mutable noexcept { AllocTraits::destroy(alloc, t); });
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename T,
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && std::is_trivially_destructible<T>{})>
    inline void destroy(const Tx, Alloc&, T*) noexcept
    {
    }

    template<typename Alloc,
             typename... Args,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(thread_data & tls_td, Alloc & alloc, Args && ... args)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args &&) args...);
        if (tls_td.in_critical_section()) {
            tls_td.after_fail([ alloc, result ]() mutable noexcept {
                AllocTraits::destroy(alloc, detail::to_raw_pointer(result));
                AllocTraits::deallocate(alloc, std::move(result), 1);
            });
        }
        return result;
    }

    template<typename Alloc,
             typename... Args,
             LSTM_REQUIRES_(!std::is_const<Alloc>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(thread_data & tls_td, Alloc & alloc, Args && ... args)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args &&) args...);
        if (tls_td.in_critical_section()) {
            tls_td.after_fail([ alloc, result ]() mutable noexcept {
                AllocTraits::deallocate(alloc, std::move(result), 1);
            });
        }
        return result;
    }

    template<typename Tx,
             typename Alloc,
             typename... Args,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && !std::is_const<Alloc>{}
                            && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(!std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(const Tx tx, Alloc& alloc, Args&&... args)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args &&) args...);
        tx.after_fail([ alloc, result ]() mutable noexcept {
            AllocTraits::destroy(alloc, detail::to_raw_pointer(result));
            AllocTraits::deallocate(alloc, std::move(result), 1);
        });
        return result;
    }

    template<typename Tx,
             typename Alloc,
             typename... Args,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && !std::is_const<Alloc>{}
                            && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             typename Pointer     = typename AllocTraits::pointer,
             LSTM_REQUIRES_(std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline Pointer allocate_construct(const Tx tx, Alloc& alloc, Args&&... args)
    {
        Pointer result = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, detail::to_raw_pointer(result), (Args &&) args...);
        tx.after_fail([ alloc, result ]() mutable noexcept {
            AllocTraits::deallocate(alloc, std::move(result), 1);
        });
        return result;
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{}
                            && !std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline void destroy_deallocate(const Tx tx, Alloc& alloc, typename AllocTraits::pointer ptr)
    {
        tx.sometime_synchronized_after([ alloc, ptr = std::move(ptr) ]() mutable noexcept {
            AllocTraits::destroy(alloc, detail::to_raw_pointer(ptr));
            AllocTraits::deallocate(alloc, std::move(ptr), 1);
        });
    }

    template<typename Tx,
             typename Alloc,
             LSTM_REQUIRES_(detail::is_transaction<Tx>{} && detail::has_value_type<Alloc>{}),
             typename AllocTraits = std::allocator_traits<Alloc>,
             LSTM_REQUIRES_(!std::is_const<Alloc>{}
                            && std::is_trivially_destructible<typename AllocTraits::value_type>{})>
    inline void destroy_deallocate(const Tx tx, Alloc& alloc, typename AllocTraits::pointer ptr)
    {
        tx.sometime_synchronized_after([ alloc, ptr = std::move(ptr) ]() mutable noexcept {
            AllocTraits::deallocate(alloc, std::move(ptr), 1);
        });
    }
LSTM_END

#endif /* LSTM_MEMORY_HPP */

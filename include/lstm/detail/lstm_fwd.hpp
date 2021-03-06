#ifndef LSTM_DETAIL_LSTM_FWD_HPP
#define LSTM_DETAIL_LSTM_FWD_HPP

// clang-format off
#ifdef NDEBUG
    #if NDEBUG != 1
        #error "NDEBUG must either be undefined or defined to be 1"
    #endif
#endif
// clang-format on

#include <lstm/detail/compiler.hpp>
#include <lstm/detail/namespace_macros.hpp>
#include <lstm/detail/perf_stats.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// from rangev3
#define LSTM_PP_CAT_(X, Y) X##Y
#define LSTM_PP_CAT(X, Y) LSTM_PP_CAT_(X, Y)

#define LSTM_REQUIRES_(...)                                                                        \
    int LSTM_PP_CAT(_concept_requires_, __LINE__)                                                  \
        = 42,                                                                                      \
        typename std::enable_if < (LSTM_PP_CAT(_concept_requires_, __LINE__) == 43)                \
            || (__VA_ARGS__),                                                                      \
        int > ::type = 0 /**/

#define LSTM_REQUIRES(...)                                                                         \
    template<int LSTM_PP_CAT(_concept_requires_, __LINE__) = 42,                                   \
             typename std::enable_if<(LSTM_PP_CAT(_concept_requires_, __LINE__) == 43)             \
                                         || (__VA_ARGS__),                                         \
                                     int>::type                                                    \
             = 0> /**/

// clang-format off
#ifndef LSTM_ALWAYS_SEQ_CST
    #define LSTM_ACQUIRE std::memory_order_acquire
    #define LSTM_RELEASE std::memory_order_release
    #define LSTM_ACQ_REL std::memory_order_acq_rel
    #define LSTM_RELAXED std::memory_order_relaxed
    #define LSTM_CONSUME std::memory_order_consume
    #define LSTM_SEQ_CST std::memory_order_seq_cst
#else
    #define LSTM_ACQUIRE std::memory_order_seq_cst
    #define LSTM_RELEASE std::memory_order_seq_cst
    #define LSTM_ACQ_REL std::memory_order_seq_cst
    #define LSTM_RELAXED std::memory_order_seq_cst
    #define LSTM_CONSUME std::memory_order_seq_cst
    #define LSTM_SEQ_CST std::memory_order_seq_cst
#endif

#ifndef LSTM_CACHE_LINE_SIZE
    #define LSTM_CACHE_LINE_SIZE 64
#endif
#ifndef LSTM_NO_CACHE_ALIGNMENT
    #define LSTM_CACHE_ALIGNED alignas(LSTM_CACHE_LINE_SIZE)
#else
    #define LSTM_CACHE_ALIGNED /**/
#endif

#ifndef LSTM_SIGNED_LOCKFREE_WORD
    #define LSTM_SIGNED_LOCKFREE_WORD std::intptr_t;
#endif
// clang-format on

LSTM_BEGIN
    // TODO verify lockfreeness of this on each platform
    using word  = LSTM_SIGNED_LOCKFREE_WORD;
    using uword = std::make_unsigned_t<word>;

    using epoch_t = uword;

    static_assert(std::is_integral<word>{}, "type chosen for word must be an integral type");
    static_assert(std::is_signed<word>{}, "type chosen for word must be signed");

    template<typename T, typename Alloc = std::allocator<std::remove_reference_t<T>>>
    struct var;

    struct transaction;
    struct read_transaction;
    struct transaction_domain;
    struct thread_data;

    template<typename T>
    struct privatized_future;
LSTM_END

LSTM_DETAIL_BEGIN
    union var_storage
    {
        void* ptr;
        char  raw[sizeof(void*)];
    };

    using hash_t = std::uint64_t;

    struct commit_algorithm;
    struct var_base;
    struct transaction_base;
    struct atomic_base_fn;

    template<std::size_t Padding>
    struct thread_synchronization_node;

    template<typename T>
    struct privatized_future_data;

    struct tx_retry
    {
    };

    template<typename T>
    constexpr const T static_const{};

    template<typename T>
    using uncvref = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename... Bs>
    using and_ = std::is_same<std::integer_sequence<bool, Bs::value..., true>,
                              std::integer_sequence<bool, true, Bs::value...>>;

    template<typename Bool>
    using not_ = std::integral_constant<bool, !Bool::value>;

    template<typename... Ts>
    struct all_same;

    template<>
    struct all_same<> : std::true_type
    {
    };

    template<typename T, typename... Ts>
    struct all_same<T, Ts...> : and_<std::is_same<T, Ts>...>
    {
    };

    template<typename...>
    using void_ = void;

    template<typename...>
    struct list
    {
    };

    template<template<typename...> class Trait, typename... Ts>
    std::false_type detector(long);

    template<template<typename...> class Trait, typename... Ts>
    std::true_type detector(decltype(std::declval<Trait<Ts...>>(), 42));

    template<template<typename...> class Trait, typename... Ts>
    using supports = decltype(lstm::detail::detector<Trait, Ts...>(42));

    template<typename T>
    void implicit_constructor_impl(T);

    template<typename To, typename... Froms>
    using is_convertible_
        = decltype(lstm::detail::implicit_constructor_impl<To>({std::declval<Froms>()...}));

    template<typename To, typename From, typename... Froms>
    using is_ilist_uncvref_convertible = decltype(lstm::detail::implicit_constructor_impl<To>(
        {std::initializer_list<uncvref<From>>{std::declval<From>(), std::declval<Froms>()...}}));

    template<typename To, typename From, typename... Froms>
    using is_ilist_const_convertible = decltype(lstm::detail::implicit_constructor_impl<To>(
        {std::initializer_list<const uncvref<From>>{std::declval<From>(),
                                                    std::declval<Froms>()...}}));

    template<typename To, typename From, typename... Froms>
    using is_ilist_volatile_convertible = decltype(lstm::detail::implicit_constructor_impl<To>(
        {std::initializer_list<volatile uncvref<From>>{std::declval<From>(),
                                                       std::declval<Froms>()...}}));

    template<typename To, typename From, typename... Froms>
    using is_ilist_cv_convertible = decltype(lstm::detail::implicit_constructor_impl<To>(
        {std::initializer_list<const volatile uncvref<From>>{std::declval<From>(),
                                                             std::declval<Froms>()...}}));

    template<typename To, typename... Froms>
    struct is_convertible;

    template<typename To>
    struct is_convertible<To> : supports<is_convertible_, To>
    {
    };

    template<typename To, typename From>
    struct is_convertible<To, From> : std::is_convertible<From, To>
    {
    };

    template<typename To, typename From0, typename From1, typename... FromTail>
    struct is_convertible<To, From0, From1, FromTail...>
        : and_<supports<is_convertible_, To, From0, From1, FromTail...>,
               std::integral_constant<bool,
                                      !all_same<uncvref<From0>,
                                                uncvref<From1>,
                                                uncvref<FromTail>...>{}
                                          || (!supports<is_ilist_uncvref_convertible,
                                                        To,
                                                        From0,
                                                        From1,
                                                        FromTail...>{}
                                              && !supports<is_ilist_const_convertible,
                                                           To,
                                                           From0,
                                                           From1,
                                                           FromTail...>{}
                                              && !supports<is_ilist_volatile_convertible,
                                                           To,
                                                           From0,
                                                           From1,
                                                           FromTail...>{}
                                              && !supports<is_ilist_cv_convertible,
                                                           To,
                                                           From0,
                                                           From1,
                                                           FromTail...>{})>>
    {
    };

    template<typename Func, typename Tx, typename... Args>
    using callable_with_tx_
        = decltype(std::declval<Func>()(std::declval<const Tx&>(), std::declval<Args>()...));

    template<typename Func, typename Tx, typename... Args>
    using callable_with_tx = supports<callable_with_tx_, Func, Tx, Args...>;

    template<typename Func, typename... Args>
    using callable_ = decltype(std::declval<Func>()(std::declval<Args>()...));

    template<typename Func, typename... Args>
    using callable = supports<callable_, Func, Args...>;

    template<bool CallableWithTx, bool Callable, typename Func, typename Tx, typename... Args>
    struct transact_result_impl;

    template<bool b, typename Func, typename Tx, typename... Args>
    struct transact_result_impl<true, b, Func, Tx, Args...>
    {
        using type
            = decltype(std::declval<Func>()(std::declval<const Tx&>(), std::declval<Args>()...));
        static constexpr bool nothrow
            = noexcept(std::declval<Func>()(std::declval<const Tx&>(), std::declval<Args>()...));
    };

    template<typename Func, typename Tx, typename... Args>
    struct transact_result_impl<false, true, Func, Tx, Args...>
    {
        using type                    = decltype(std::declval<Func>()(std::declval<Args>()...));
        static constexpr bool nothrow = noexcept(std::declval<Func>()(std::declval<Args>()...));
    };
    template<typename Func, typename Tx, typename... Args>
    using transact_result = typename transact_result_impl<callable_with_tx<Func, Tx, Args...>{}(),
                                                          callable<Func, Args...>{}(),
                                                          Func,
                                                          Tx,
                                                          Args...>::type;

    template<typename Func, typename Tx, typename... Args>
    using transact_nothrow
        = std::enable_if_t<transact_result_impl<callable_with_tx<Func, Tx, Args...>{}(),
                                                callable<Func, Args...>{}(),
                                                Func,
                                                Tx,
                                                Args...>::nothrow>;

    template<typename Func, typename Tx, typename... Args>
    using is_void_transact_function = std::is_void<transact_result<Func, Tx, Args...>>;

    template<typename Func, typename Tx, typename... Args>
    using is_transact_function_ = supports<transact_result, Func, Tx, Args...>;

    template<typename Func, typename Tx, typename... Args>
    using is_nothrow_transact_function = supports<transact_nothrow, Func, Tx, Args...>;

    template<typename Func, typename Tx, typename... Args>
    using is_transact_function
        = and_<is_transact_function_<Func, Tx, Args...>,
               not_<is_nothrow_transact_function<Func, Tx, Args...>>,
               is_transact_function_<uncvref<Func>&, Tx, Args...>,
               not_<is_nothrow_transact_function<uncvref<Func>&, Tx, Args...>>>;

    template<typename T>
    using uninitialized = std::aligned_storage_t<sizeof(T), alignof(T)>;

    template<typename T>
    inline T* to_raw_pointer(T * p) noexcept
    {
        return p;
    }

    // clang-format off
    template<typename Pointer, LSTM_REQUIRES_(!std::is_pointer<uncvref<Pointer>>())>
    inline typename std::pointer_traits<uncvref<Pointer>>::element_type*
    to_raw_pointer(Pointer&& p) noexcept
    {
        return p != nullptr ? detail::to_raw_pointer(p.operator->()) : nullptr;
    }
// clang-format on
LSTM_DETAIL_END

#endif /* LSTM_DETAIL_LSTM_FWD_HPP */

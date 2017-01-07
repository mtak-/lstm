#ifndef LSTM_DETAIL_LSTM_FWD_HPP
#define LSTM_DETAIL_LSTM_FWD_HPP

#ifdef NDEBUG
    #if NDEBUG != 1
        #error "NDEBUG must either be undefined or defined to be 1"
    #endif
#endif

#include <lstm/detail/compiler.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#ifndef NDEBUG
    #define LSTM_BEGIN namespace lstm { inline namespace v1 { inline namespace debug {
    #define LSTM_END }}}
#else
    #define LSTM_BEGIN namespace lstm { inline namespace v1 {
    #define LSTM_END }}
#endif /* NDEBUG */

#define LSTM_DETAIL_BEGIN LSTM_BEGIN namespace detail {
#define LSTM_DETAIL_END LSTM_END }

#define LSTM_TEST_BEGIN LSTM_BEGIN namespace test {
#define LSTM_TEST_END LSTM_END }

// from rangev3
#define LSTM_PP_CAT_(X, Y) X ## Y
#define LSTM_PP_CAT(X, Y)  LSTM_PP_CAT_(X, Y)

#define LSTM_REQUIRES_(...)                                                                        \
    int LSTM_PP_CAT(_concept_requires_, __LINE__) = 42,                                            \
    typename std::enable_if<                                                                       \
        (LSTM_PP_CAT(_concept_requires_, __LINE__) == 43) || (__VA_ARGS__),                        \
        int                                                                                        \
    >::type = 0                                                                                    \
    /**/
    
#define LSTM_REQUIRES(...)                                                                         \
    template<                                                                                      \
        int LSTM_PP_CAT(_concept_requires_, __LINE__) = 42,                                        \
        typename std::enable_if<                                                                   \
            (LSTM_PP_CAT(_concept_requires_, __LINE__) == 43) || (__VA_ARGS__),                    \
            int                                                                                    \
        >::type = 0>                                                                               \
    /**/
    
#ifdef LSTM_LOG_TRANSACTIONS
    #include <lstm/detail/transaction_log.hpp>
    
    #define LSTM_INTERNAL_FAIL_TX() lstm::detail::transaction_log::get().add_internal_failure()
    #define LSTM_USER_FAIL_TX() lstm::detail::transaction_log::get().add_user_failure()
    #define LSTM_SUCC_TX() lstm::detail::transaction_log::get().add_success()
    #define LSTM_LOG_REGISTER_THREAD_ID(id) lstm::detail::transaction_log::get().register_thread(id)
    #define LSTM_LOG_CLEAR() lstm::detail::transaction_log::get().clear()
    #ifndef LSTM_LOG_DUMP
        #include <iostream>
        #define LSTM_LOG_DUMP() (std::cout << lstm::detail::transaction_log::get().results())
    #endif /* LSTM_LOG_DUMP */
#else
    #define LSTM_INTERNAL_FAIL_TX() /**/
    #define LSTM_USER_FAIL_TX() /**/
    #define LSTM_SUCC_TX() /**/
    #define LSTM_LOG_REGISTER_THREAD_ID(id) /**/
    #define LSTM_LOG_CLEAR() /**/
    #ifndef LSTM_LOG_DUMP
        #define LSTM_LOG_DUMP() /**/
    #endif /* LSTM_LOG_DUMP */
#endif /* LSTM_LOG_TRANSACTIONS */

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

LSTM_BEGIN
    // TODO verify lockfreeness of this on each platform
    using word = LSTM_SIGNED_LOCKFREE_WORD;
    using uword = std::make_unsigned_t<word>;
    
    using gp_t = uword;
    using mutex_type = std::mutex;
    
    static_assert(std::is_integral<word>{}, "type chosen for word must be an integral type");
    static_assert(std::is_signed<word>{}, "type chosen for word must be signed");
    
    template<typename T, typename Alloc = std::allocator<std::remove_reference_t<T>>>
    struct var;
    
    struct transaction;
    struct transaction_domain;
    struct thread_data;
LSTM_END

LSTM_DETAIL_BEGIN
    using var_storage = void*;
    
    struct read_write_fn;
    struct var_base;
    
    struct _tx_retry {};
    
    template<typename T>
    using uncvref = std::remove_cv_t<std::remove_reference_t<T>>;
    
    template<typename... Bs>
    using and_ = std::is_same<
        std::integer_sequence<bool, Bs::value..., true>,
        std::integer_sequence<bool, true, Bs::value...>>;
    
    template<typename Bool>
    using not_ = std::integral_constant<bool, !Bool::value>;
    
    template<typename... Ts>
    struct all_same;
    
    template<>
    struct all_same<> : std::true_type {};
    
    template<typename T, typename... Ts>
    struct all_same<T, Ts...> : and_<std::is_same<T, Ts>...> {};
    
    template<typename...>
    using void_ = void;
    
    template<typename...>
    struct list {};
    
    template<template<typename...> class Trait, typename... Ts>
    std::false_type detector(long);
    
    template<template<typename...> class Trait, typename... Ts>
    std::true_type detector(decltype(std::declval<Trait<Ts...>>(), 42));
    
    template<template<typename...> class Trait, typename... Ts>
    using supports = decltype(lstm::detail::detector<Trait, Ts...>(42));
    
    template<typename T>
    void implicit_constructor_impl(T);
    
    template<typename To, typename... Froms>
    using is_convertible_ =
        decltype(lstm::detail::implicit_constructor_impl<To>({std::declval<Froms>()...}));
        
    template<typename To, typename From, typename... Froms>
    using is_ilist_uncvref_convertible =
        decltype(lstm::detail::implicit_constructor_impl<To>(
                    {std::initializer_list<uncvref<From>>{std::declval<From>(),
                                                          std::declval<Froms>()...}}));
    
    template<typename To, typename From, typename... Froms>
    using is_ilist_const_convertible =
        decltype(lstm::detail::implicit_constructor_impl<To>(
                    {std::initializer_list<const uncvref<From>>{std::declval<From>(),
                                                                std::declval<Froms>()...}}));
    
    template<typename To, typename From, typename... Froms>
    using is_ilist_volatile_convertible =
        decltype(lstm::detail::implicit_constructor_impl<To>(
                    {std::initializer_list<volatile uncvref<From>>{std::declval<From>(),
                                                                   std::declval<Froms>()...}}));
       
       template<typename To, typename From, typename... Froms>
       using is_ilist_cv_convertible =
           decltype(lstm::detail::implicit_constructor_impl<To>(
                       {std::initializer_list<const volatile uncvref<From>>{std::declval<From>(),
                                                                       std::declval<Froms>()...}}));
    
    template<typename To, typename... Froms>
    struct is_convertible;
    
    template<typename To>
    struct is_convertible<To> : supports<is_convertible_, To> {};
    
    template<typename To, typename From>
    struct is_convertible<To, From> : std::is_convertible<From, To> {};
    
    template<typename To, typename From0, typename From1, typename... FromTail>
    struct is_convertible<To, From0, From1, FromTail...>
        : and_<supports<is_convertible_, To, From0, From1, FromTail...>,
               std::integral_constant<bool,
                    (!supports<is_ilist_uncvref_convertible, To, From0, From1, FromTail...>{} &&
                     !supports<is_ilist_const_convertible, To, From0, From1, FromTail...>{} &&
                     !supports<is_ilist_volatile_convertible, To, From0, From1, FromTail...>{} &&
                     !supports<is_ilist_cv_convertible, To, From0, From1, FromTail...>{}) ||
                    !all_same<uncvref<From0>, uncvref<From1>, uncvref<FromTail>...>{}>>
    {};
    
    template<typename Func, typename Tx, typename = void>
    struct callable_with_tx : std::false_type {};
    
    template<typename Func, typename Tx>
    struct callable_with_tx<Func, Tx, void_<decltype(std::declval<Func>()(std::declval<Tx>()))>>
        : std::true_type {};
    
    template<typename Func, typename = void>
    struct callable : std::false_type {};
    
    template<typename Func>
    struct callable<Func, void_<decltype(std::declval<Func>()())>>
        : std::true_type {};
    
    template<typename Func, bool = callable_with_tx<Func&, transaction&>{}(),
                            bool = callable<Func&>{}()>
    struct transact_result_impl;

    template<typename Func, bool b>
    struct transact_result_impl<Func, true, b> {
        using type = decltype(std::declval<Func&>()(std::declval<transaction&>()));
        static constexpr bool nothrow =
            noexcept(std::declval<Func&>()(std::declval<transaction&>()));
    };
    
    template<typename Func>
    struct transact_result_impl<Func, false, true> {
        using type = decltype(std::declval<Func&>()());
        static constexpr bool nothrow = noexcept(std::declval<Func&>()());
    };

    template<typename Func>
    using transact_result = typename transact_result_impl<Func>::type;

    template<typename Func>
    constexpr bool transact_nothrow = transact_result_impl<Func>::nothrow;

    template<typename Func, typename = void>
    struct is_void_transact_function : std::false_type {};

    template<typename Func>
    struct is_void_transact_function<Func, std::enable_if_t<std::is_void<transact_result<Func>>{}>>
        : std::true_type {};
        
    template<typename Func, typename = void>
    struct is_transact_function : std::false_type {};

    template<typename Func>
    struct is_transact_function<Func, void_<transact_result<Func>>> : std::true_type {};
    
    template<typename Func, typename = void>
    struct is_nothrow_transact_function : std::false_type {};

    template<typename Func>
    struct is_nothrow_transact_function<Func, std::enable_if_t<transact_nothrow<Func>>>
        : std::true_type {};

    template<typename T>
    using uninitialized = std::aligned_storage_t<sizeof(T), alignof(T)>;
    
    template<typename T> inline T* to_raw_pointer(T* p) noexcept { return p; }
    template<typename Pointer,
        LSTM_REQUIRES_(!std::is_pointer<uncvref<Pointer>>())>
    inline typename std::pointer_traits<uncvref<Pointer>>::element_type*
    to_raw_pointer(Pointer&& p) noexcept {
        return p != nullptr
            ? detail::to_raw_pointer(p.operator->())
            : nullptr;
    }
LSTM_DETAIL_END

LSTM_TEST_BEGIN
    struct transaction_tester;
LSTM_TEST_END

#endif /* LSTM_DETAIL_LSTM_FWD_HPP */

#ifndef LSTM_DETAIL_LSTM_FWD_HPP
#define LSTM_DETAIL_LSTM_FWD_HPP

#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#define LSTM_BEGIN namespace lstm { inline namespace v1 {
#define LSTM_END }}

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

LSTM_DETAIL_BEGIN
    using var_storage = void*;
    struct atomic_fn;
    struct var_base;
    template<typename Alloc>
    struct transaction_impl;
    struct tx_retry {};
    
    struct write_set_lookup {
    private:
        var_storage* _pending_write;
        
    public:
        inline constexpr write_set_lookup(std::nullptr_t) noexcept : _pending_write{nullptr} {}
        
        inline constexpr
        write_set_lookup(var_storage& in_pending_write) noexcept
            : _pending_write{&in_pending_write}
        {}
            
        inline constexpr bool success() const noexcept { return _pending_write != nullptr; }
        inline constexpr var_storage& pending_write() const noexcept { return *_pending_write; }
    };
LSTM_DETAIL_END

LSTM_BEGIN
    // TODO verify lockfreeness of this on each platform
    using word = std::uintptr_t;
    
    template<typename T, typename Alloc = std::allocator<std::remove_reference_t<T>>>
    struct var;
    
    struct transaction;
    
    template<typename T>
    using uncvref = std::remove_cv_t<std::remove_reference_t<T>>;
    
    template<typename... Bs>
    using and_ = std::is_same<
        std::integer_sequence<bool, Bs::value..., true>,
        std::integer_sequence<bool, true, Bs::value...>>;
LSTM_END

LSTM_TEST_BEGIN
    struct transaction_tester;
LSTM_TEST_END

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

#endif /* LSTM_DETAIL_LSTM_FWD_HPP */
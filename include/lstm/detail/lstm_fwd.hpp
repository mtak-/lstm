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

LSTM_BEGIN
    // TODO verify lockfreeness of this on each platform
    using word = std::uintptr_t;
    
    template<typename T>
    struct var;
    
    namespace detail {
        struct atomic_fn;
        struct var_base;
    }
    
    template<typename Alloc = std::allocator<detail::var_base*>>
    struct transaction;
    
    template<typename T>
    struct var_proxy;
    
    struct tx_retry {};
    
    template<typename T>
    using uncvref = std::remove_cv_t<std::remove_reference_t<T>>;
    
    namespace detail {
        template<typename T>
        struct is_var_ : std::false_type {};
        
        template<typename T>
        struct is_var_<var<T>> : std::true_type {};
    }
    
    template<typename T>
    using is_var = detail::is_var_<uncvref<T>>;
    
    template<typename... Bs>
    using and_ = std::is_same<
        std::integer_sequence<bool, Bs::value..., true>,
        std::integer_sequence<bool, true, Bs::value...>>;
LSTM_END

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
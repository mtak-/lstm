#ifndef LSTM_DETAIL_NAMESPACE_MACROS_HPP
#define LSTM_DETAIL_NAMESPACE_MACROS_HPP

// clang-format off
#ifndef LSTM_LOG_TRANSACTIONS
    #define LSTM__LOG_BEGIN /**/
    #define LSTM__LOG_END /**/
#else
    #define LSTM__LOG_BEGIN inline namespace logged {
    #define LSTM__LOG_END }
#endif

#ifndef NDEBUG
    #define LSTM_BEGIN                                                                             \
        namespace lstm { inline namespace v1 { inline namespace debug { LSTM__LOG_BEGIN            \
    /**/
    #define LSTM_END LSTM__LOG_END }}}
#else
    #define LSTM_BEGIN namespace lstm { inline namespace v1 { LSTM__LOG_BEGIN
    #define LSTM_END LSTM__LOG_END }}
#endif /* NDEBUG */

#define LSTM_DETAIL_BEGIN LSTM_BEGIN namespace detail {
#define LSTM_DETAIL_END LSTM_END }

#define LSTM_TEST_BEGIN LSTM_BEGIN namespace test {
#define LSTM_TEST_END LSTM_END }
// clang-format on

#endif /* LSTM_DETAIL_NAMESPACE_MACROS_HPP */
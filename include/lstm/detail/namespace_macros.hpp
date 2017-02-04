#ifndef LSTM_DETAIL_NAMESPACE_MACROS_HPP
#define LSTM_DETAIL_NAMESPACE_MACROS_HPP

// clang-format off
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
// clang-format on

#endif /* LSTM_DETAIL_NAMESPACE_MACROS_HPP */
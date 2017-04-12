#include <lstm/thread_data.hpp>

int main() { return 0; }

static_assert(std::is_standard_layout<lstm::thread_data>{}, "");
static_assert(alignof(lstm::thread_data) == LSTM_CACHE_LINE_SIZE, "");
static_assert(sizeof(lstm::thread_data) % LSTM_CACHE_LINE_SIZE == 0, "");

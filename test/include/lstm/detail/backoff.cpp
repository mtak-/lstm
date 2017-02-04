#include <lstm/detail/backoff.hpp>

int main() { return 0; }

static_assert(
    lstm::detail::is_backoff_strategy<lstm::detail::exponential_delay<std::chrono::nanoseconds,
                                                                      100000,
                                                                      10000000>>{},
    "");
static_assert(lstm::detail::is_backoff_strategy<lstm::detail::yield>{}, "");
#include "util/logger.h"

#include <chrono>

namespace aemu::log::detail
{
long long monotonic_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace aemu::log::detail

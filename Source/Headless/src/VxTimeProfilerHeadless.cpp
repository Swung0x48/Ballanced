#include "VxTimeProfiler.h"

#include <chrono>
#include <cstring>

namespace {
using HeadlessClock = std::chrono::steady_clock;

uint64_t NowNanoseconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            HeadlessClock::now().time_since_epoch()).count());
}
}

VxTimeProfiler &VxTimeProfiler::operator=(const VxTimeProfiler &other) {
    if (&other != this) {
        std::memcpy(Times, other.Times, sizeof(Times));
    }
    return *this;
}

void VxTimeProfiler::Reset() {
    const uint64_t now = NowNanoseconds();
    std::memcpy(&Times[0], &now, sizeof(now));
    const uint64_t zero = 0;
    std::memcpy(&Times[2], &zero, sizeof(zero));
}

float VxTimeProfiler::Current() {
    uint64_t start = 0;
    std::memcpy(&start, &Times[0], sizeof(start));
    const uint64_t elapsed = NowNanoseconds() - start;
    std::memcpy(&Times[2], &elapsed, sizeof(elapsed));
    return static_cast<float>(static_cast<double>(elapsed) / 1000000.0);
}

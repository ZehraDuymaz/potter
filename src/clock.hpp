#pragma once

#include <chrono>
#include <ctime>

namespace potter {

// A monotonic clock that keeps counting while the machine is suspended
// (Linux CLOCK_BOOTTIME). steady_clock stops during sleep, and system_clock
// jumps when the wall time is adjusted; this does neither.
struct BootClock {
  using duration = std::chrono::nanoseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<BootClock>;
  static constexpr bool is_steady = true;

  static time_point now() noexcept {
    timespec ts{};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return time_point(std::chrono::seconds(ts.tv_sec) +
                      std::chrono::nanoseconds(ts.tv_nsec));
  }
};

}  // namespace potter

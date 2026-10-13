#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "core/clock.hpp"

namespace potter {

enum class Phase { Focus, ShortBreak, LongBreak };

const char* phase_name(Phase phase);

struct TimerSettings {
  std::chrono::minutes focus{25};
  std::chrono::minutes short_break{5};
  std::chrono::minutes long_break{15};
  int sessions_before_long_break = 4;
  bool auto_start_breaks = false;
  bool auto_start_focus = false;
};

// Pure Pomodoro state machine. It never reads the clock itself: every call
// that depends on time takes `now`, which keeps it easy to test.
class Timer {
 public:
  using Clock = BootClock;
  using Duration = std::chrono::nanoseconds;

  explicit Timer(TimerSettings settings = {});

  void start(Clock::time_point now);
  void pause(Clock::time_point now);
  void toggle(Clock::time_point now);
  // Puts the current phase back to its full length and stops.
  void reset();
  // Moves to the next phase without counting the current one as completed.
  void skip();

  // Call regularly. Returns the phase that just finished, if one did.
  std::optional<Phase> update(Clock::time_point now);

  // Changes the settings. A phase that has not been started yet picks up
  // its new length; a running or paused one keeps its time.
  void apply_settings(const TimerSettings& settings);

  Phase phase() const { return phase_; }
  bool running() const { return running_; }
  // True when the current phase hasn't been started (full time left, stopped).
  bool untouched() const;
  Duration remaining(Clock::time_point now) const;
  // Length of the current phase, fixed when the phase began.
  Duration phase_length() const { return phase_length_; }
  // Time spent in the current phase so far.
  Duration elapsed(Clock::time_point now) const { return phase_length_ - remaining(now); }
  // Changes whenever a new phase begins (including reset and skip).
  std::uint64_t phase_id() const { return phase_id_; }
  // Focus sessions completed in the current cycle (resets after a long break).
  int cycle_position() const { return cycle_pos_; }
  // Focus sessions completed since the timer was created.
  int completed_focus() const { return completed_focus_; }
  const TimerSettings& settings() const { return settings_; }

 private:
  Duration length_of(Phase phase) const;
  void advance(bool completed);
  void begin_phase();  // stop, with the full length of the current phase

  TimerSettings settings_;
  Phase phase_ = Phase::Focus;
  bool running_ = false;
  Clock::time_point end_time_{};  // valid while running
  Duration remaining_{};          // valid while stopped
  Duration phase_length_{};
  std::uint64_t phase_id_ = 0;
  int cycle_pos_ = 0;
  int completed_focus_ = 0;
};

// Measures focus time as it happens, so it can be credited to a task.
// Pauses, resets and skips are handled: only time actually spent in a
// focus phase is counted, and never more than the phase's length.
class FocusMeter {
 public:
  // Returns the focus time that passed since the previous call.
  Timer::Duration collect(const Timer& timer, Timer::Clock::time_point now);

 private:
  std::uint64_t phase_id_ = 0;
  Timer::Duration counted_{};
};

}  // namespace potter

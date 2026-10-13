// Unit tests for the Pomodoro state machine. No framework: each CHECK
// prints the failing line, and the exit code tells CTest the result.

#include <cstdio>

#include "core/config.hpp"
#include "core/timer.hpp"

using namespace potter;
using namespace std::chrono_literals;

static int failures = 0;

#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using Clock = Timer::Clock;
static const Clock::time_point t0{};

static void starts_untouched_in_focus() {
  Timer timer;
  CHECK(timer.phase() == Phase::Focus);
  CHECK(!timer.running());
  CHECK(timer.untouched());
  CHECK(timer.remaining(t0) == 25min);
}

static void counts_down_and_pauses() {
  Timer timer;
  timer.start(t0);
  CHECK(timer.remaining(t0 + 10min) == 15min);
  timer.pause(t0 + 10min);
  // Time passing while paused doesn't count.
  CHECK(timer.remaining(t0 + 1h) == 15min);
  timer.start(t0 + 1h);
  CHECK(timer.remaining(t0 + 1h + 5min) == 10min);
}

static void finishes_focus_into_short_break() {
  Timer timer;
  timer.start(t0);
  CHECK(!timer.update(t0 + 24min));
  auto finished = timer.update(t0 + 25min);
  CHECK(finished && *finished == Phase::Focus);
  CHECK(timer.phase() == Phase::ShortBreak);
  CHECK(!timer.running());  // auto-start is off by default
  CHECK(timer.cycle_position() == 1);
  CHECK(timer.completed_focus() == 1);
}

static void long_break_after_n_sessions() {
  TimerSettings s;
  s.focus = 1min;
  s.short_break = 1min;
  s.long_break = 2min;
  s.sessions_before_long_break = 2;
  s.auto_start_breaks = true;
  s.auto_start_focus = true;
  Timer timer(s);

  auto now = t0;
  timer.start(now);
  now += 1min;
  CHECK(timer.update(now) == Phase::Focus);
  CHECK(timer.phase() == Phase::ShortBreak);
  CHECK(timer.running());
  now += 1min;
  CHECK(timer.update(now) == Phase::ShortBreak);
  now += 1min;
  CHECK(timer.update(now) == Phase::Focus);
  CHECK(timer.phase() == Phase::LongBreak);
  CHECK(timer.cycle_position() == 2);
  now += 2min;
  CHECK(timer.update(now) == Phase::LongBreak);
  CHECK(timer.phase() == Phase::Focus);
  CHECK(timer.cycle_position() == 0);
  CHECK(timer.completed_focus() == 2);
}

static void oversleeping_finishes_one_phase_at_a_time() {
  TimerSettings s;
  s.auto_start_breaks = true;
  s.auto_start_focus = true;
  Timer timer(s);
  timer.start(t0);
  // Laptop slept for 3 hours: only the focus phase completes, and the break
  // starts from the wake-up time.
  const auto wake = t0 + 3h;
  CHECK(timer.update(wake) == Phase::Focus);
  CHECK(timer.phase() == Phase::ShortBreak);
  CHECK(timer.remaining(wake) == 5min);
  CHECK(!timer.update(wake));
}

static void skip_does_not_count() {
  Timer timer;
  timer.start(t0);
  timer.skip();
  CHECK(timer.phase() == Phase::ShortBreak);
  CHECK(!timer.running());
  CHECK(timer.completed_focus() == 0);
  CHECK(timer.cycle_position() == 0);
  timer.skip();
  CHECK(timer.phase() == Phase::Focus);
}

static void reset_restores_full_length() {
  Timer timer;
  timer.start(t0);
  timer.reset();
  CHECK(!timer.running());
  CHECK(timer.remaining(t0 + 5min) == 25min);
}

static void apply_settings_only_changes_untouched_phase() {
  Timer timer;
  TimerSettings s;
  s.focus = 50min;
  timer.apply_settings(s);
  CHECK(timer.remaining(t0) == 50min);

  timer.start(t0);
  s.focus = 10min;
  timer.apply_settings(s);
  CHECK(timer.remaining(t0 + 5min) == 45min);  // running phase keeps its time
  CHECK(timer.phase_length() == 50min);        // so the progress bar doesn't jump
  timer.reset();
  CHECK(timer.remaining(t0) == 10min);         // reset picks up the new length
}

static void hex_colors() {
  auto c = parse_hex_color("#ff8000");
  CHECK(c && c->r == 255 && c->g == 128 && c->b == 0);
  CHECK(parse_hex_color("ABCDEF"));
  CHECK(!parse_hex_color("#fff"));
  CHECK(!parse_hex_color("#gg0000"));
}

static void focus_meter_counts_only_focus_time() {
  TimerSettings s;
  s.auto_start_breaks = true;
  Timer timer(s);
  FocusMeter meter;

  CHECK(meter.collect(timer, t0) == 0min);  // not started yet
  timer.start(t0);
  CHECK(meter.collect(timer, t0 + 10min) == 10min);
  CHECK(meter.collect(timer, t0 + 10min) == 0min);  // nothing new
  timer.pause(t0 + 12min);
  CHECK(meter.collect(timer, t0 + 1h) == 2min);  // paused time doesn't count
  timer.start(t0 + 1h);
  // Laptop slept past the end: only the rest of the 25 minutes counts.
  CHECK(meter.collect(timer, t0 + 5h) == 13min);
  timer.update(t0 + 5h);
  CHECK(timer.phase() == Phase::ShortBreak);
  CHECK(meter.collect(timer, t0 + 5h + 3min) == 0min);  // breaks don't count
}

static void focus_meter_handles_reset_and_skip() {
  Timer timer;
  FocusMeter meter;
  timer.start(t0);
  CHECK(meter.collect(timer, t0 + 5min) == 5min);
  timer.reset();  // the 5 minutes already counted stay counted
  timer.start(t0 + 5min);
  CHECK(meter.collect(timer, t0 + 8min) == 3min);
  timer.skip();
  CHECK(meter.collect(timer, t0 + 9min) == 0min);
}

int main() {
  starts_untouched_in_focus();
  counts_down_and_pauses();
  finishes_focus_into_short_break();
  long_break_after_n_sessions();
  oversleeping_finishes_one_phase_at_a_time();
  skip_does_not_count();
  reset_restores_full_length();
  apply_settings_only_changes_untouched_phase();
  hex_colors();
  focus_meter_counts_only_focus_time();
  focus_meter_handles_reset_and_skip();

  if (failures == 0) std::printf("all timer tests passed\n");
  return failures == 0 ? 0 : 1;
}

#include "timer.hpp"

#include <algorithm>

namespace potter {

const char* phase_name(Phase phase) {
  switch (phase) {
    case Phase::Focus: return "Focus";
    case Phase::ShortBreak: return "Short break";
    case Phase::LongBreak: return "Long break";
  }
  return "";
}

Timer::Timer(TimerSettings settings) : settings_(settings) { begin_phase(); }

void Timer::start(Clock::time_point now) {
  if (running_) return;
  end_time_ = now + remaining_;
  running_ = true;
}

void Timer::pause(Clock::time_point now) {
  if (!running_) return;
  remaining_ = std::max(Duration::zero(), Duration(end_time_ - now));
  running_ = false;
}

void Timer::toggle(Clock::time_point now) {
  if (running_) {
    pause(now);
  } else {
    start(now);
  }
}

void Timer::reset() { begin_phase(); }

void Timer::skip() {
  advance(/*completed=*/false);
  begin_phase();
}

std::optional<Phase> Timer::update(Clock::time_point now) {
  if (!running_ || now < end_time_) return std::nullopt;

  const Phase finished = phase_;
  advance(/*completed=*/true);
  begin_phase();

  const bool auto_start = phase_ == Phase::Focus ? settings_.auto_start_focus
                                                 : settings_.auto_start_breaks;
  // Start from `now`, not the old end time, so waking from a long sleep
  // doesn't race through several phases at once.
  if (auto_start) start(now);
  return finished;
}

void Timer::apply_settings(const TimerSettings& settings) {
  const bool was_untouched = untouched();
  settings_ = settings;
  settings_.sessions_before_long_break =
      std::max(1, settings_.sessions_before_long_break);
  if (was_untouched) begin_phase();
}

bool Timer::untouched() const {
  return !running_ && remaining_ == phase_length_;
}

Timer::Duration Timer::remaining(Clock::time_point now) const {
  if (!running_) return remaining_;
  return std::max(Duration::zero(), Duration(end_time_ - now));
}

Timer::Duration Timer::length_of(Phase phase) const {
  switch (phase) {
    case Phase::Focus: return settings_.focus;
    case Phase::ShortBreak: return settings_.short_break;
    case Phase::LongBreak: return settings_.long_break;
  }
  return settings_.focus;
}

void Timer::begin_phase() {
  ++phase_id_;
  running_ = false;
  phase_length_ = length_of(phase_);
  remaining_ = phase_length_;
}

void Timer::advance(bool completed) {
  if (phase_ == Phase::Focus) {
    if (completed) {
      ++cycle_pos_;
      ++completed_focus_;
    }
    phase_ = cycle_pos_ >= settings_.sessions_before_long_break
                 ? Phase::LongBreak
                 : Phase::ShortBreak;
  } else {
    if (phase_ == Phase::LongBreak) cycle_pos_ = 0;
    phase_ = Phase::Focus;
  }
}

Timer::Duration FocusMeter::collect(const Timer& timer, Timer::Clock::time_point now) {
  if (timer.phase_id() != phase_id_) {
    phase_id_ = timer.phase_id();
    counted_ = Timer::Duration::zero();
  }
  if (timer.phase() != Phase::Focus) return Timer::Duration::zero();

  const auto elapsed = timer.elapsed(now);
  if (elapsed <= counted_) return Timer::Duration::zero();
  const auto delta = elapsed - counted_;
  counted_ = elapsed;
  return delta;
}

}  // namespace potter

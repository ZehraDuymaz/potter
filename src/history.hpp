#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <string>

namespace potter {

using Day = std::chrono::sys_days;

// The day a moment belongs to, in local time. Days start at
// `day_start_hour` (0-12): with 4, a session at 1 AM still counts for the
// day before.
Day day_of(std::chrono::system_clock::time_point t, int day_start_hour);

// "2026-10-13"
std::string format_day(Day day);

struct DayStats {
  std::chrono::nanoseconds focus{};
  int sessions = 0;      // completed focus phases
  int goal_minutes = 0;  // the daily goal that applied on this day

  bool goal_met() const {
    return goal_minutes > 0 && focus >= std::chrono::minutes(goal_minutes);
  }
};

// Focus time per day, for the daily goal, streaks and the history chart.
// Pure logic, no UI.
class History {
 public:
  // Adds focus time to `day` and records the goal for that day. Returns true
  // if this is what pushed the day over its goal.
  bool add_focus(Day day, std::chrono::nanoseconds amount, int goal_minutes);
  void add_session(Day day, int goal_minutes);
  // Updates the goal of a day that already has an entry (e.g. today, after
  // the goal is changed in settings). Past days keep their own goal.
  void set_goal(Day day, int goal_minutes);

  // Empty stats for a day without any focus.
  DayStats get(Day day) const;
  // Consecutive days that met their goal, ending today — or ending
  // yesterday while today's goal isn't met yet, so a streak isn't "lost"
  // in the morning.
  int streak(Day today) const;
  int best_streak() const;

  bool dirty() const { return dirty_; }

  // A missing file is an empty history, not an error.
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  bool save(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  std::map<Day, DayStats> days_;
  bool dirty_ = false;
};

// $XDG_DATA_HOME/potter/history.toml, or ~/.local/share/potter/history.toml.
std::filesystem::path history_path();

}  // namespace potter

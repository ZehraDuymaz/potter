#include "core/history.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <optional>
#include <sstream>
#include <system_error>

#include <toml++/toml.hpp>

#include "core/storage.hpp"

namespace potter {

using namespace std::chrono;

Day day_of(system_clock::time_point t, int day_start_hour) {
  // Shift back by the start hour, then take the local calendar date.
  const std::time_t shifted = system_clock::to_time_t(t) - day_start_hour * 3600;
  std::tm tm{};
  localtime_r(&shifted, &tm);
  return Day(year(tm.tm_year + 1900) / month(tm.tm_mon + 1) / day(tm.tm_mday));
}

std::string format_day(Day d) {
  const year_month_day ymd(d);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", static_cast<int>(ymd.year()),
                static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()));
  return buf;
}

bool History::add_focus(Day d, nanoseconds amount, int goal_minutes) {
  if (amount <= amount.zero()) return false;
  DayStats& stats = days_[d];
  const bool was_met = stats.goal_met();
  stats.focus += amount;
  stats.goal_minutes = goal_minutes;
  dirty_ = true;
  return !was_met && stats.goal_met();
}

void History::add_session(Day d, int goal_minutes) {
  DayStats& stats = days_[d];
  ++stats.sessions;
  stats.goal_minutes = goal_minutes;
  dirty_ = true;
}

void History::set_goal(Day d, int goal_minutes) {
  const auto it = days_.find(d);
  if (it == days_.end() || it->second.goal_minutes == goal_minutes) return;
  it->second.goal_minutes = goal_minutes;
  dirty_ = true;
}

DayStats History::get(Day d) const {
  const auto it = days_.find(d);
  return it == days_.end() ? DayStats{} : it->second;
}

int History::streak(Day today) const {
  Day d = today;
  if (!get(d).goal_met()) d -= days(1);  // today is still in progress
  int count = 0;
  while (get(d).goal_met()) {
    ++count;
    d -= days(1);
  }
  return count;
}

int History::best_streak() const {
  int best = 0;
  int run = 0;
  std::optional<Day> previous;
  for (const auto& [d, stats] : days_) {
    if (!stats.goal_met()) {
      run = 0;
    } else {
      run = (previous && *previous + days(1) == d && run > 0) ? run + 1 : 1;
    }
    best = std::max(best, run);
    previous = d;
  }
  return best;
}

bool History::load(const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return true;

  toml::table root;
  try {
    root = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    if (error) *error = "history.toml: " + std::string(e.description());
    return false;
  }

  std::map<Day, DayStats> loaded;
  if (const toml::array* list = root["day"].as_array()) {
    for (const toml::node& node : *list) {
      const toml::table* t = node.as_table();
      if (!t) continue;
      const auto date = (*t)["date"].value<toml::date>();
      if (!date) continue;
      const year_month_day ymd{year(date->year), month(date->month), day(date->day)};
      if (!ymd.ok()) continue;

      DayStats stats;
      stats.focus = seconds(std::max<int64_t>(0, (*t)["focus_seconds"].value_or(int64_t{0})));
      stats.sessions = static_cast<int>((*t)["sessions"].value_or(int64_t{0}));
      stats.goal_minutes = static_cast<int>((*t)["goal_minutes"].value_or(int64_t{0}));
      loaded[Day(ymd)] = stats;
    }
  }
  days_ = std::move(loaded);
  dirty_ = false;
  return true;
}

bool History::save(const std::filesystem::path& path, std::string* error) {
  toml::array list;
  for (const auto& [d, stats] : days_) {
    const year_month_day ymd(d);
    list.push_back(toml::table{
        {"date", toml::date{static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                            static_cast<unsigned>(ymd.day())}},
        {"focus_seconds", static_cast<int64_t>(duration_cast<seconds>(stats.focus).count())},
        {"sessions", stats.sessions},
        {"goal_minutes", stats.goal_minutes},
    });
  }
  toml::table root{{"day", std::move(list)}};

  std::ostringstream out;
  out << "# potter history — focus time per day\n\n" << root << '\n';
  if (!write_atomically(path, out.str(), error)) return false;
  dirty_ = false;
  return true;
}

std::filesystem::path history_path() {
  return xdg_dir("XDG_DATA_HOME", ".local/share") / "potter" / "history.toml";
}

}  // namespace potter

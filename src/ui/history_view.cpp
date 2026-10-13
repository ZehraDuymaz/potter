#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

constexpr int kDays = 14;    // columns in the chart
constexpr int kHeight = 10;  // rows of bar height
constexpr int kAxisWidth = 8;

// Partial blocks for the top of a bar, from empty to full.
constexpr std::array<const char*, 9> kEighths = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
constexpr std::array<const char*, 7> kWeekdays = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

std::string repeat(const std::string& s, int n) {
  std::string out;
  for (int i = 0; i < n; ++i) out += s;
  return out;
}

std::string pad_left(const std::string& s, int width) {
  const int len = static_cast<int>(s.size());  // labels are ASCII
  return len >= width ? s : std::string(width - len, ' ') + s;
}

double to_minutes(std::chrono::nanoseconds d) {
  return std::chrono::duration<double, std::chrono::minutes::period>(d).count();
}

std::string format_minutes(double minutes) {
  if (minutes <= 0) return "0m";
  return format_focus(std::chrono::minutes(static_cast<long long>(std::lround(minutes))));
}

}  // namespace

Element history_view(const History& history, Day today, const Config& config) {
  using namespace std::chrono;

  const Color bar_color = phase_color(Phase::Focus, config);
  const Color met_color = phase_color(Phase::ShortBreak, config);
  const double goal = config.daily_goal_minutes;

  std::array<Day, kDays> dates;
  std::array<DayStats, kDays> stats;
  std::array<double, kDays> minutes_per_day;
  double highest = goal;
  double total = 0;
  int days_met = 0;
  for (int i = 0; i < kDays; ++i) {
    dates[i] = today - days(kDays - 1 - i);
    stats[i] = history.get(dates[i]);
    minutes_per_day[i] = to_minutes(stats[i].focus);
    highest = std::max(highest, minutes_per_day[i]);
    total += minutes_per_day[i];
    if (stats[i].goal_met()) ++days_met;
  }
  // Round the top of the scale up to a half hour, so the axis label is tidy.
  const double top = std::ceil(highest / 30.0) * 30.0;

  Elements rows;
  for (int level = kHeight; level >= 1; --level) {
    const double hi = top * level / kHeight;
    const double lo = top * (level - 1) / kHeight;
    const bool goal_row = goal > lo && goal <= hi;

    const std::string label = level == kHeight ? format_minutes(top)
                              : goal_row       ? format_minutes(goal)
                                               : "";
    Elements cells;
    cells.push_back(text(pad_left(label, kAxisWidth - 2) + (label.empty() ? " │" : " ┤")) | dim);

    for (int i = 0; i < kDays; ++i) {
      const double v = minutes_per_day[i];
      std::string cell;
      if (v >= hi) {
        cell = "██";
      } else if (v > lo) {
        const int eighths = std::clamp(static_cast<int>(std::lround((v - lo) / (hi - lo) * 8)), 1, 8);
        cell = repeat(kEighths[eighths], 2);
      }

      // The goal line runs through the gaps and empty cells as a dashed line.
      cells.push_back(text(goal_row ? "╌" : " ") | dim);
      if (cell.empty()) {
        cells.push_back(text(goal_row ? "╌╌" : "  ") | dim);
      } else {
        cells.push_back(text(cell) | color(stats[i].goal_met() ? met_color : bar_color));
      }
    }
    rows.push_back(hbox(std::move(cells)));
  }

  // X axis with weekday and day-of-month labels; today is highlighted.
  rows.push_back(text(std::string(kAxisWidth - 1, ' ') + "└" + repeat("─", kDays * 3)) | dim);
  Elements weekday_labels{text(std::string(kAxisWidth, ' '))};
  Elements date_labels{text(std::string(kAxisWidth, ' '))};
  for (int i = 0; i < kDays; ++i) {
    const unsigned wd = weekday(dates[i]).c_encoding();
    const unsigned dom = static_cast<unsigned>(year_month_day(dates[i]).day());
    std::string number = (dom < 10 ? "0" : "") + std::to_string(dom);
    const bool is_today = i == kDays - 1;
    Decorator style = is_today ? Decorator(bold) : Decorator(dim);
    weekday_labels.push_back(text(" " + std::string(kWeekdays[wd])) | style);
    date_labels.push_back(text(" " + number) | style);
  }
  rows.push_back(hbox(std::move(weekday_labels)));
  rows.push_back(hbox(std::move(date_labels)));

  const int streak = history.streak(today);
  const int best = std::max(history.best_streak(), streak);
  const DayStats today_stats = history.get(today);

  Element header = hbox({
      text("History") | bold,
      filler(),
      text(streak > 0 ? "🔥 streak " + std::to_string(streak) : "no streak yet"),
      text(" · best " + std::to_string(best)) | dim,
  });

  Element summary = vbox({
      hbox({text("today ") | dim,
            text(format_minutes(to_minutes(today_stats.focus)) + " of " + format_minutes(goal)) |
                (today_stats.goal_met() ? color(met_color) : nothing),
            text(today_stats.goal_met() ? " ✓ goal reached" : "")}),
      text("last " + std::to_string(kDays) + " days: " + format_minutes(total) +
           " · goal met on " + std::to_string(days_met) + (days_met == 1 ? " day" : " days")) |
          dim,
  });

  return vbox({
             header,
             separator(),
             text(""),
             vbox(std::move(rows)),
             text(""),
             summary,
             separator(),
             text("press any key to close") | dim | hcenter,
         }) |
         borderRounded | center;
}

}  // namespace potter::ui

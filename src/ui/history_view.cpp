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
constexpr int kMinWidth = 64;
constexpr int kMaxWidth = 90;
// Columns of a task row: title · time · status.
constexpr int kTimeWidth = 8;
constexpr int kStatusWidth = 10;
constexpr int kGap = 3;

// Partial blocks for the top of a bar, from empty to full.
constexpr std::array<const char*, 9> kEighths = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
constexpr std::array<const char*, 7> kWeekdays = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
constexpr std::array<const char*, 7> kWeekdayNames = {"Sun", "Mon", "Tue", "Wed",
                                                      "Thu", "Fri", "Sat"};
constexpr std::array<const char*, 12> kMonths = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string repeat(const std::string& s, int n) {
  std::string out;
  for (int i = 0; i < n; ++i) out += s;
  return out;
}

double to_minutes(std::chrono::nanoseconds d) {
  return std::chrono::duration<double, std::chrono::minutes::period>(d).count();
}

std::string format_minutes(double minutes) {
  if (minutes <= 0) return "0m";
  return format_focus(std::chrono::minutes(static_cast<long long>(std::lround(minutes))));
}

// Width of the title column in a task row: indent · title · time · status.
int task_title_width(int inner) {
  return std::max(10, inner - 2 - kGap - kTimeWidth - kGap - kStatusWidth);
}

std::string format_amount(std::chrono::nanoseconds d) {
  const std::string s = format_focus(d);
  return s.empty() ? "0m" : s;
}

// "Mon 13 Oct"
std::string day_title(Day d) {
  using namespace std::chrono;
  const year_month_day ymd(d);
  return std::string(kWeekdayNames[weekday(d).c_encoding()]) + " " +
         std::to_string(static_cast<unsigned>(ymd.day())) + " " +
         kMonths[static_cast<unsigned>(ymd.month()) - 1];
}

}  // namespace

HistoryScreen::HistoryScreen(const History& history, const TaskList& tasks, const Config& config)
    : history_(history), tasks_(tasks), config_(config) {}

void HistoryScreen::open() {
  done_ = false;
  tab_ = Tab::Days;
  day_offset_ = 0;
  scroll_ = 0;
}

bool HistoryScreen::handle(const Event& event) {
  if (event == Event::Escape || event == Event::Character('q')) {
    done_ = true;
  } else if (event == Event::Tab) {
    tab_ = tab_ == Tab::Days ? Tab::Tasks : Tab::Days;
    scroll_ = 0;
  } else if (tab_ == Tab::Days) {
    if (event == Event::ArrowLeft || event == Event::Character('h')) {
      day_offset_ = std::min(day_offset_ + 1, kDays - 1);
    } else if (event == Event::ArrowRight || event == Event::Character('l')) {
      day_offset_ = std::max(day_offset_ - 1, 0);
    }
  } else {
    if (event == Event::Character('r')) {
      range_ = range_ == Range::Week ? Range::Month : range_ == Range::Month ? Range::All
                                                                             : Range::Week;
      scroll_ = 0;
    } else if (event == Event::Character('j') || event == Event::ArrowDown) {
      ++scroll_;  // clamped when rendering
    } else if (event == Event::Character('k') || event == Event::ArrowUp) {
      scroll_ = std::max(scroll_ - 1, 0);
    }
  }
  return true;
}

Element HistoryScreen::task_row(const TaskTime& entry, int inner) const {
  // The live task list knows about renames and whether a task is done; a
  // task that's gone keeps the title saved in the history.
  std::string title;
  std::string status;
  bool muted = false;
  if (entry.id == 0) {
    title = "no task";
    muted = true;
  } else if (const Task* task = tasks_.find(entry.id)) {
    title = task->title;
    if (task->done) {
      status = "done";
    } else if (tasks_.active() == task->id) {
      status = "active";
    } else {
      status = "open";  // not finished, not being worked on right now
    }
  } else {
    title = entry.title.empty() ? "(untitled)" : entry.title;
    status = "deleted";
    muted = true;
  }

  const int title_width = task_title_width(inner);
  Element name = text(fit(title, title_width)) | size(WIDTH, EQUAL, title_width);
  if (muted) name = name | dim;
  return hbox({
      text("  "),
      name,
      text(std::string(kGap, ' ')),
      text(pad_left(format_amount(entry.focus), kTimeWidth)),
      text(std::string(kGap, ' ')),
      text(status) | dim | size(WIDTH, EQUAL, kStatusWidth),
  });
}

Element HistoryScreen::render_chart(Day today) const {
  using namespace std::chrono;

  const Color bar_color = phase_color(Phase::Focus, config_);
  const Color met_color = phase_color(Phase::ShortBreak, config_);
  const double goal = config_.daily_goal_minutes;
  const int selected = kDays - 1 - day_offset_;

  std::array<Day, kDays> dates;
  std::array<DayStats, kDays> stats;
  std::array<double, kDays> minutes_per_day;
  double highest = goal;
  for (int i = 0; i < kDays; ++i) {
    dates[i] = today - days(kDays - 1 - i);
    stats[i] = history_.get(dates[i]);
    minutes_per_day[i] = to_minutes(stats[i].focus);
    highest = std::max(highest, minutes_per_day[i]);
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

  // X axis with weekday and day-of-month labels; the selected day is
  // highlighted and marked with ▲.
  rows.push_back(text(std::string(kAxisWidth - 1, ' ') + "└" + repeat("─", kDays * 3)) | dim);
  Elements weekday_labels{text(std::string(kAxisWidth, ' '))};
  Elements date_labels{text(std::string(kAxisWidth, ' '))};
  for (int i = 0; i < kDays; ++i) {
    const unsigned wd = weekday(dates[i]).c_encoding();
    const unsigned dom = static_cast<unsigned>(year_month_day(dates[i]).day());
    const std::string number = (dom < 10 ? "0" : "") + std::to_string(dom);
    Decorator style = i == selected ? Decorator(bold) : Decorator(dim);
    weekday_labels.push_back(text(" "));
    weekday_labels.push_back(text(kWeekdays[wd]) | style);
    date_labels.push_back(text(" "));
    date_labels.push_back(text(number) | style | (i == selected ? inverted : nothing));
  }
  rows.push_back(hbox(std::move(weekday_labels)));
  rows.push_back(hbox(std::move(date_labels)));
  rows.push_back(text(std::string(kAxisWidth + selected * 3 + 1, ' ') + "▲") |
                 color(bar_color));
  return vbox(std::move(rows));
}

Element HistoryScreen::render_day(Day day, Day today, int inner) const {
  const DayStats stats = history_.get(day);
  std::string heading = day_title(day);
  if (day == today) heading += " · today";

  Elements lines;
  lines.push_back(hbox({
      text(heading) | bold,
      filler(),
      text("total focus: ") | dim,
      text(format_amount(stats.focus)) | (stats.goal_met() ? bold : nothing),
      text(stats.goal_met() ? " ✓" : ""),
  }));
  lines.push_back(text(""));  // a little room between the day and its tasks

  const auto breakdown = history_.task_breakdown(day);
  if (stats.focus <= stats.focus.zero()) {
    lines.push_back(text("  no focus recorded") | dim);
  } else if (breakdown.empty()) {
    lines.push_back(text("  no per-task details for this day (recorded before v0.5)") | dim);
  } else {
    for (const TaskTime& entry : breakdown) lines.push_back(task_row(entry, inner));
  }
  return vbox(std::move(lines));
}

Element HistoryScreen::render_totals(Day today, int inner) const {
  using namespace std::chrono;
  Day first = today;
  std::string range_label;
  switch (range_) {
    case Range::Week:
      first = today - days(6);
      range_label = "last 7 days";
      break;
    case Range::Month:
      first = today - days(29);
      range_label = "last 30 days";
      break;
    case Range::All:
      first = history_.first_day().value_or(today);
      range_label = "all time";
      break;
  }

  const auto totals = history_.task_totals(first, today);
  std::chrono::nanoseconds sum{};
  for (const TaskTime& t : totals) sum += t.focus;
  const int selected = totals.empty() ? 0 : std::min(scroll_, static_cast<int>(totals.size()) - 1);

  Elements rows;
  for (int i = 0; i < static_cast<int>(totals.size()); ++i) {
    Element row = task_row(totals[i], inner);
    if (i == selected) row = row | focus;  // keeps the selected row in view
    rows.push_back(row);
  }
  if (rows.empty()) {
    rows.push_back(text("  no per-task focus recorded yet — make a task active with enter") |
                   dim);
  }

  return vbox({
      // Column titles line up with task_row(): "focused time" ends where the
      // time column ends, and "status" starts where the status column starts.
      // Both are measured from the left, with the same widths.
      [&] {
        const std::string label = "focused time";
        const int label_end = 2 + task_title_width(inner) + kGap + kTimeWidth;
        const int left = std::max(0, label_end - static_cast<int>(label.size()));
        return hbox({text(fit("Task totals · " + range_label, left)) | bold |
                         size(WIDTH, EQUAL, left),
                     text(label) | dim,
                     text(std::string(kGap, ' ')),
                     text("status") | dim});
      }(),
      text(""),
      vbox(std::move(rows)) | vscroll_indicator | yframe | flex,
      text(""),
      hbox({text("  total") | dim, filler(), text(format_amount(sum)) | bold}),
  });
}

Element HistoryScreen::render(Day today) const {
  using namespace std::chrono;
  const int width = screen_width(kMinWidth, kMaxWidth);
  const int inner = width - 2 - 2 * kPadX;

  const int streak = history_.streak(today);
  const int best = std::max(history_.best_streak(), streak);
  auto tab_label = [&](const char* name, Tab tab) {
    return tab_ == tab ? text(std::string("[ ") + name + " ]") | bold
                       : text(std::string("  ") + name + "  ") | dim;
  };

  Element header = hbox({
      text("History") | bold,
      text("   "),
      tab_label("Days", Tab::Days),
      tab_label("Tasks", Tab::Tasks),
      filler(),
      text(streak > 0 ? "🔥 streak " + std::to_string(streak) : "no streak yet"),
      text(" · best " + std::to_string(best)) | dim,
  });

  Element body;
  Element footer;
  if (tab_ == Tab::Days) {
    // Totals for the two weeks in the chart.
    nanoseconds total{};
    int days_met = 0;
    for (int i = 0; i < kDays; ++i) {
      const DayStats s = history_.get(today - days(i));
      total += s.focus;
      if (s.goal_met()) ++days_met;
    }
    body = vbox({
        render_chart(today),
        text(""),
        render_day(today - days(day_offset_), today, inner),
        text(""),
        text("last " + std::to_string(kDays) + " days: " + format_amount(total) +
             " · goal met on " + std::to_string(days_met) + (days_met == 1 ? " day" : " days")) |
            dim,
    });
    footer = text("←/→ pick a day · tab task totals · esc close") | dim;
  } else {
    body = render_totals(today, inner);
    footer = text("j/k scroll · r change range · tab days · esc close") | dim;
  }

  return padded_box(vbox({
                        header,
                        separator(),
                        text(""),
                        body | flex,
                        separator(),
                        footer,
                    }),
                    width);
}

}  // namespace potter::ui

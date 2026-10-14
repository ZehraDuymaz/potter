// Unit tests for focus history, goals and streaks. Same style as timer_test.cpp.

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

#include "core/history.hpp"

using namespace potter;
using namespace std::chrono;
using namespace std::chrono_literals;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static const Day kToday = 2026y / October / 13;

static void reaching_the_goal_is_reported_once() {
  History h;
  CHECK(!h.add_focus(kToday, 60min, 100));
  CHECK(!h.get(kToday).goal_met());
  CHECK(h.add_focus(kToday, 40min, 100));  // this crosses the goal
  CHECK(h.get(kToday).goal_met());
  CHECK(!h.add_focus(kToday, 10min, 100));  // already met
  CHECK(h.get(kToday).focus == 110min);
  CHECK(!h.add_focus(kToday, 0min, 100));
}

static void counts_sessions() {
  History h;
  h.add_session(kToday, 100);
  h.add_session(kToday, 100);
  CHECK(h.get(kToday).sessions == 2);
  CHECK(h.get(kToday - days(1)).sessions == 0);  // empty day
}

static void streak_rules() {
  History h;
  // Met the goal 3 days in a row up to yesterday, with a gap before that.
  h.add_focus(kToday - days(5), 120min, 100);
  h.add_focus(kToday - days(3), 100min, 100);
  h.add_focus(kToday - days(2), 100min, 100);
  h.add_focus(kToday - days(1), 100min, 100);
  CHECK(h.streak(kToday) == 3);  // today in progress doesn't break it

  h.add_focus(kToday, 30min, 100);
  CHECK(h.streak(kToday) == 3);  // still not met today
  h.add_focus(kToday, 70min, 100);
  CHECK(h.streak(kToday) == 4);  // today counts once met

  // Tomorrow, without any focus, the streak is still alive (it ends "yesterday").
  CHECK(h.streak(kToday + days(1)) == 4);
  // But a full missed day breaks it.
  CHECK(h.streak(kToday + days(2)) == 0);
  CHECK(h.best_streak() == 4);
}

static void each_day_keeps_its_own_goal() {
  History h;
  h.add_focus(kToday - days(1), 60min, 50);  // the goal was 50 min back then
  h.add_focus(kToday, 60min, 100);
  CHECK(h.get(kToday - days(1)).goal_met());
  CHECK(!h.get(kToday).goal_met());
  h.set_goal(kToday, 60);  // goal lowered in settings: today is now met
  CHECK(h.get(kToday).goal_met());
  CHECK(h.streak(kToday) == 2);
  h.set_goal(kToday + days(1), 10);  // no entry: nothing is created
  CHECK(h.get(kToday + days(1)).goal_minutes == 0);
}

static void best_streak_needs_consecutive_days() {
  History h;
  h.add_focus(kToday - days(10), 100min, 100);
  h.add_focus(kToday - days(9), 100min, 100);
  h.add_focus(kToday - days(8), 10min, 100);  // not met
  h.add_focus(kToday - days(7), 100min, 100);
  h.add_focus(kToday - days(5), 100min, 100);  // gap on day 6
  CHECK(h.best_streak() == 2);
}

static void day_starts_at_the_configured_hour() {
  // Use UTC so the test doesn't depend on the machine's time zone.
  setenv("TZ", "UTC", 1);
  tzset();
  const auto one_am = sys_days(kToday) + 1h;
  CHECK(day_of(one_am, 4) == kToday - days(1));  // still "yesterday" at 1 AM
  CHECK(day_of(one_am, 0) == kToday);            // midnight start
  CHECK(day_of(sys_days(kToday) + 4h, 4) == kToday);
  CHECK(day_of(sys_days(kToday) + 23h + 59min, 4) == kToday);
  CHECK(format_day(kToday) == "2026-10-13");
}

static void saves_and_loads() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-history-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "history.toml";

  History h;
  CHECK(h.load(path));  // missing file = empty history
  h.add_focus(kToday - days(1), 100min, 100);
  h.add_focus(kToday, 45min + 30s, 120);
  h.add_session(kToday, 120);
  CHECK(h.dirty());
  CHECK(h.save(path));
  CHECK(!h.dirty());

  History loaded;
  CHECK(loaded.load(path));
  CHECK(loaded.get(kToday).focus == 45min + 30s);
  CHECK(loaded.get(kToday).sessions == 1);
  CHECK(loaded.get(kToday).goal_minutes == 120);
  CHECK(loaded.get(kToday - days(1)).goal_met());
  CHECK(loaded.streak(kToday) == 1);

  std::ofstream(path) << "day = [ broken";
  std::string error;
  CHECK(!loaded.load(path, &error));
  CHECK(!error.empty());

  std::filesystem::remove_all(dir);
}

static void credits_focus_to_tasks() {
  History h;
  h.add_focus(kToday, 20min, 100, 1, "Study chapter 4");
  h.add_focus(kToday, 10min, 100);  // no active task
  h.add_focus(kToday, 25min, 100, 2, "Write report");
  h.add_focus(kToday, 30min, 100, 1, "Study ch. 4 (renamed)");
  CHECK(h.get(kToday).focus == 85min);  // the day total still adds up

  const auto day = h.task_breakdown(kToday);
  CHECK(day.size() == 3);
  CHECK(day[0].id == 1);  // most time first
  CHECK(day[0].focus == 50min);
  CHECK(day[0].title == "Study ch. 4 (renamed)");  // keeps the latest title
  CHECK(day[1].id == 2 && day[1].focus == 25min);
  CHECK(day[2].id == 0 && day[2].focus == 10min);  // the "no task" bucket
  CHECK(h.task_breakdown(kToday - days(1)).empty());
}

static void totals_cover_only_the_range() {
  History h;
  h.add_focus(kToday - days(40), 60min, 100, 7, "Old project");
  h.add_focus(kToday - days(10), 30min, 100, 1, "Study");
  h.add_focus(kToday - days(3), 15min, 100, 1, "Study");
  h.add_focus(kToday, 45min, 100, 2, "Report");

  const auto week = h.task_totals(kToday - days(6), kToday);
  CHECK(week.size() == 2);
  CHECK(week[0].id == 2 && week[0].focus == 45min);
  CHECK(week[1].id == 1 && week[1].focus == 15min);

  const auto month = h.task_totals(kToday - days(29), kToday);
  CHECK(month.size() == 2);
  CHECK(month[0].id == 1 && month[0].focus == 45min);  // 30 + 15

  // "All time" still has the old project, even though that task is long gone.
  const auto all = h.task_totals(*h.first_day(), kToday);
  CHECK(all.size() == 3);
  CHECK(all[0].id == 7 && all[0].title == "Old project" && all[0].focus == 60min);
  CHECK(*h.first_day() == kToday - days(40));
  CHECK(!History{}.first_day());
}

static void task_details_survive_save_and_load() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-history-tasks-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "history.toml";

  History h;
  h.add_focus(kToday, 25min, 100, 3, "Çalışma — ünite 3");
  h.add_focus(kToday, 5min, 100);
  CHECK(h.save(path));

  History loaded;
  CHECK(loaded.load(path));
  const auto day = loaded.task_breakdown(kToday);
  CHECK(day.size() == 2);
  CHECK(day[0].id == 3 && day[0].title == "Çalışma — ünite 3" && day[0].focus == 25min);
  CHECK(day[1].id == 0 && day[1].focus == 5min);

  // Files from v0.3/v0.4 have no task details and still load fine.
  std::ofstream(path) << "[[day]]\ndate = 2026-10-12\nfocus_seconds = 6000\nsessions = 4\n"
                         "goal_minutes = 100\n";
  CHECK(loaded.load(path));
  CHECK(loaded.get(kToday - days(1)).goal_met());
  CHECK(loaded.task_breakdown(kToday - days(1)).empty());
  std::filesystem::remove_all(dir);
}

static void removes_a_task_everywhere() {
  History h;
  h.add_focus(kToday - days(2), 30min, 100, 9, "test run");  // only test focus that day
  h.add_focus(kToday - days(1), 100min, 100, 1, "Study");
  h.add_focus(kToday - days(1), 10min, 100, 9, "test run");
  h.add_focus(kToday, 20min, 100, 9, "test run");
  h.add_focus(kToday, 5min, 100);  // no task
  CHECK(h.save(std::filesystem::temp_directory_path() / "potter-remove-test.toml"));
  CHECK(!h.dirty());

  CHECK(h.remove_task(9) == 60min);  // 30 + 10 + 20
  CHECK(h.dirty());
  // The day that only had test focus is gone completely.
  CHECK(h.get(kToday - days(2)).focus == 0min);
  CHECK(*h.first_day() == kToday - days(1));
  // Other days lose just that task's time, from the total too.
  CHECK(h.get(kToday - days(1)).focus == 100min);
  CHECK(h.get(kToday - days(1)).goal_met());
  CHECK(h.task_breakdown(kToday - days(1)).size() == 1);
  CHECK(h.get(kToday).focus == 5min);
  CHECK(h.task_totals(kToday - days(6), kToday).size() == 2);  // Study + no task

  CHECK(h.remove_task(0) == 5min);  // the "no task" bucket can go too
  CHECK(h.get(kToday).focus == 0min);
  CHECK(h.remove_task(42) == 0min);  // unknown task: nothing happens
  std::filesystem::remove(std::filesystem::temp_directory_path() / "potter-remove-test.toml");
}

int main() {
  removes_a_task_everywhere();
  credits_focus_to_tasks();
  totals_cover_only_the_range();
  task_details_survive_save_and_load();
  reaching_the_goal_is_reported_once();
  counts_sessions();
  streak_rules();
  each_day_keeps_its_own_goal();
  best_streak_needs_consecutive_days();
  day_starts_at_the_configured_hour();
  saves_and_loads();

  if (failures == 0) std::printf("all history tests passed\n");
  return failures == 0 ? 0 : 1;
}

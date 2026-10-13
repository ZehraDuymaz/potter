// Unit tests for the task list. Same style as timer_test.cpp.

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "core/tasks.hpp"

using namespace potter;
using namespace std::chrono_literals;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static std::vector<std::string> titles(const TaskList& list) {
  std::vector<std::string> out;
  for (const TaskRow& row : list.rows()) {
    out.push_back(std::string(row.depth, '-') + row.task->title);
  }
  return out;
}

static void adds_tasks_and_subtasks() {
  TaskList list;
  const TaskId a = list.add("  write report  ");
  const TaskId b = list.add("study");
  const TaskId a1 = list.add("outline", a);
  CHECK(list.add("   ") == 0);  // blank titles are ignored
  CHECK(list.find(a)->title == "write report");  // trimmed
  // A subtask of a subtask goes under the top-level parent.
  const TaskId a2 = list.add("draft", a1);
  CHECK(list.find(a2)->parent == a);
  CHECK(list.find(b)->parent == 0);
  CHECK((titles(list) == std::vector<std::string>{"write report", "-outline", "-draft", "study"}));
  CHECK(list.size() == 4);
}

static void removes_with_subtasks() {
  TaskList list;
  const TaskId a = list.add("a");
  list.add("a1", a);
  const TaskId b = list.add("b");
  list.set_active(a);
  CHECK(list.remove(a));
  CHECK((titles(list) == std::vector<std::string>{"b"}));
  CHECK(list.active() == 0);  // the active task is gone
  CHECK(!list.remove(a));
  CHECK(list.find(b));
}

static void moves_among_siblings() {
  TaskList list;
  const TaskId a = list.add("a");
  const TaskId a1 = list.add("a1", a);
  const TaskId a2 = list.add("a2", a);
  const TaskId b = list.add("b");
  CHECK(list.move(b, -1));
  CHECK((titles(list) == std::vector<std::string>{"b", "a", "-a1", "-a2"}));
  CHECK(!list.move(b, -1));  // already first
  CHECK(list.move(a2, -1));
  CHECK((titles(list) == std::vector<std::string>{"b", "a", "-a2", "-a1"}));
  CHECK(!list.move(a1, +1));  // already last under its parent
}

static void done_and_active() {
  TaskList list;
  const TaskId a = list.add("a");
  list.set_active(a);
  CHECK(list.active() == a);
  CHECK(list.toggle_done(a));
  CHECK(list.find(a)->done);
  CHECK(list.active() == 0);  // done tasks stop collecting time
  CHECK(list.count_done() == 1);
  list.set_active(12345);  // unknown ids clear the active task
  CHECK(list.active() == 0);
}

static void focus_rolls_up_to_parent() {
  TaskList list;
  const TaskId a = list.add("a");
  const TaskId a1 = list.add("a1", a);
  list.add_focus(a, 10min);
  list.add_focus(a1, 25min);
  CHECK(list.total_focus(a) == 35min);
  CHECK(list.total_focus(a1) == 25min);
}

static void saves_and_loads() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-tasks-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "tasks.toml";

  TaskList list;
  CHECK(list.load(path));  // missing file = empty list
  CHECK(list.size() == 0);

  const TaskId a = list.add("çalış — ünite 3");  // UTF-8 survives the round trip
  const TaskId a1 = list.add("sub", a);
  list.toggle_done(a1);
  list.add_focus(a, 90s);
  list.set_active(a);
  CHECK(list.dirty());
  CHECK(list.save(path));
  CHECK(!list.dirty());

  TaskList loaded;
  CHECK(loaded.load(path));
  CHECK((titles(loaded) == titles(list)));
  CHECK(loaded.active() == a);
  CHECK(loaded.find(a1)->done);
  CHECK(loaded.find(a)->focus == 90s);
  // New ids never reuse old ones.
  CHECK(loaded.add("new") > a1);

  std::filesystem::remove_all(dir);
}

static void repairs_hand_edited_files() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-tasks-test2";
  std::filesystem::create_directories(dir);
  const auto path = dir / "tasks.toml";
  std::ofstream(path) << R"(
active = 99
[[task]]
id = 1
title = "orphan"
parent = 42

[[task]]
id = 2
title = ""
)";
  TaskList list;
  CHECK(list.load(path));
  CHECK(list.size() == 1);              // the blank task is dropped
  CHECK(list.find(1)->parent == 0);     // missing parent → top-level
  CHECK(list.active() == 0);            // unknown active id is cleared
  CHECK(list.add("next") == 2);

  std::ofstream(path) << "this is = = not toml";
  std::string error;
  CHECK(!list.load(path, &error));
  CHECK(!error.empty());

  std::filesystem::remove_all(dir);
}

static void formats_focus_time() {
  CHECK(format_focus(0s) == "");
  CHECK(format_focus(30s) == "<1m");
  CHECK(format_focus(25min) == "25m");
  CHECK(format_focus(65min) == "1h 05m");
  CHECK(format_focus(10h) == "10h 00m");
}

int main() {
  adds_tasks_and_subtasks();
  removes_with_subtasks();
  moves_among_siblings();
  done_and_active();
  focus_rolls_up_to_parent();
  saves_and_loads();
  repairs_hand_edited_files();
  formats_focus_time();

  if (failures == 0) std::printf("all task tests passed\n");
  return failures == 0 ? 0 : 1;
}

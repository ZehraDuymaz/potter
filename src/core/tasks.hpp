#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace potter {

using TaskId = std::uint64_t;  // 0 means "no task"

enum class Priority { None, Low, Medium, High };

// "none", "low", "medium", "high".
const char* priority_name(Priority priority);

struct Task {
  TaskId id = 0;
  TaskId parent = 0;  // 0 for a top-level task
  std::string title;
  bool done = false;
  Priority priority = Priority::None;
  std::chrono::nanoseconds focus{};  // focus time spent on this task itself
};

// One line of the task list as shown on screen.
struct TaskRow {
  const Task* task;
  int depth;  // 0 = top-level, 1 = subtask
};

// The to-do list. Tasks can have one level of subtasks; a subtask's focus
// time also counts toward its parent. Pure logic, no UI.
class TaskList {
 public:
  // Adds a task and returns its id, or 0 if the title is blank. A subtask
  // of a subtask is attached to that subtask's parent instead.
  TaskId add(std::string title, TaskId parent = 0);
  // Removes a task and its subtasks.
  bool remove(TaskId id);
  bool rename(TaskId id, std::string title);
  // Marking the active task done also clears the active task.
  bool toggle_done(TaskId id);
  // Swaps a task with its previous (-1) or next (+1) sibling. With
  // `same_priority`, only siblings of the same priority count, so a move
  // stays inside its group when the list is sorted by priority.
  bool move(TaskId id, int direction, bool same_priority = false);
  bool set_priority(TaskId id, Priority priority);
  // Steps up (+1) or down (-1) through none → low → medium → high, wrapping.
  bool cycle_priority(TaskId id, int direction);
  void add_focus(TaskId id, std::chrono::nanoseconds amount);

  const Task* find(TaskId id) const;
  // Own focus time plus that of its subtasks.
  std::chrono::nanoseconds total_focus(TaskId id) const;
  // Tasks in display order: each top-level task followed by its subtasks.
  // `by_priority` puts higher priorities first (among top-level tasks, and
  // among the subtasks of each task), keeping your own order within a level.
  std::vector<TaskRow> rows(bool by_priority = false) const;
  int size() const { return static_cast<int>(tasks_.size()); }
  int count_done() const;

  // The task that focus time is credited to (0 for none).
  TaskId active() const { return active_; }
  void set_active(TaskId id);

  // Whether anything changed since the last load or save.
  bool dirty() const { return dirty_; }

  // A missing file is an empty list, not an error.
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  bool save(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  Task* find_mut(TaskId id);

  std::vector<Task> tasks_;
  TaskId next_id_ = 1;
  TaskId active_ = 0;
  bool dirty_ = false;
};

// $XDG_DATA_HOME/potter/tasks.toml, or ~/.local/share/potter/tasks.toml.
std::filesystem::path tasks_path();

// "1h 05m", "25m", "<1m" — or "" for no time at all.
std::string format_focus(std::chrono::nanoseconds amount);

}  // namespace potter

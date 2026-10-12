#include "tasks.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <sstream>
#include <system_error>
#include <utility>

#include <toml++/toml.hpp>

#include "storage.hpp"

namespace potter {

namespace {

std::string trim(std::string s) {
  const auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

}  // namespace

TaskId TaskList::add(std::string title, TaskId parent) {
  title = trim(std::move(title));
  if (title.empty()) return 0;

  if (const Task* p = find(parent)) {
    parent = p->parent != 0 ? p->parent : p->id;
  } else {
    parent = 0;
  }

  Task task;
  task.id = next_id_++;
  task.parent = parent;
  task.title = std::move(title);
  tasks_.push_back(std::move(task));
  dirty_ = true;
  return tasks_.back().id;
}

bool TaskList::remove(TaskId id) {
  if (!find(id)) return false;
  std::erase_if(tasks_, [id](const Task& t) { return t.id == id || t.parent == id; });
  if (!find(active_)) active_ = 0;
  dirty_ = true;
  return true;
}

bool TaskList::rename(TaskId id, std::string title) {
  Task* task = find_mut(id);
  title = trim(std::move(title));
  if (!task || title.empty()) return false;
  task->title = std::move(title);
  dirty_ = true;
  return true;
}

bool TaskList::toggle_done(TaskId id) {
  Task* task = find_mut(id);
  if (!task) return false;
  task->done = !task->done;
  // A finished task shouldn't keep collecting focus time.
  if (task->done && active_ == id) active_ = 0;
  dirty_ = true;
  return true;
}

bool TaskList::move(TaskId id, int direction) {
  const auto it = std::find_if(tasks_.begin(), tasks_.end(),
                               [id](const Task& t) { return t.id == id; });
  if (it == tasks_.end() || direction == 0) return false;

  const TaskId parent = it->parent;
  const auto same_parent = [parent](const Task& t) { return t.parent == parent; };

  if (direction < 0) {
    const auto rit = std::find_if(std::make_reverse_iterator(it), tasks_.rend(), same_parent);
    if (rit == tasks_.rend()) return false;
    std::iter_swap(it, std::prev(rit.base()));
  } else {
    const auto next = std::find_if(std::next(it), tasks_.end(), same_parent);
    if (next == tasks_.end()) return false;
    std::iter_swap(it, next);
  }
  dirty_ = true;
  return true;
}

void TaskList::add_focus(TaskId id, std::chrono::nanoseconds amount) {
  if (Task* task = find_mut(id); task && amount > amount.zero()) {
    task->focus += amount;
    dirty_ = true;
  }
}

const Task* TaskList::find(TaskId id) const {
  if (id == 0) return nullptr;
  for (const Task& t : tasks_) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

Task* TaskList::find_mut(TaskId id) {
  return const_cast<Task*>(std::as_const(*this).find(id));
}

std::chrono::nanoseconds TaskList::total_focus(TaskId id) const {
  std::chrono::nanoseconds total{};
  for (const Task& t : tasks_) {
    if (t.id == id || t.parent == id) total += t.focus;
  }
  return total;
}

std::vector<TaskRow> TaskList::rows() const {
  std::vector<TaskRow> rows;
  rows.reserve(tasks_.size());
  for (const Task& top : tasks_) {
    if (top.parent != 0) continue;
    rows.push_back({&top, 0});
    for (const Task& sub : tasks_) {
      if (sub.parent == top.id) rows.push_back({&sub, 1});
    }
  }
  return rows;
}

int TaskList::count_done() const {
  return static_cast<int>(std::count_if(tasks_.begin(), tasks_.end(),
                                        [](const Task& t) { return t.done; }));
}

void TaskList::set_active(TaskId id) {
  const TaskId next = find(id) ? id : 0;
  if (next == active_) return;
  active_ = next;
  dirty_ = true;
}

bool TaskList::load(const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return true;

  toml::table root;
  try {
    root = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    if (error) *error = "tasks.toml: " + std::string(e.description());
    return false;
  }

  std::vector<Task> loaded;
  TaskId max_id = 0;
  if (const toml::array* list = root["task"].as_array()) {
    for (const toml::node& node : *list) {
      const toml::table* t = node.as_table();
      if (!t) continue;
      Task task;
      task.id = static_cast<TaskId>((*t)["id"].value_or(int64_t{0}));
      task.title = trim((*t)["title"].value_or(std::string()));
      if (task.id == 0 || task.title.empty()) continue;
      task.parent = static_cast<TaskId>((*t)["parent"].value_or(int64_t{0}));
      task.done = (*t)["done"].value_or(false);
      task.focus = std::chrono::seconds((*t)["focus_seconds"].value_or(int64_t{0}));
      max_id = std::max(max_id, task.id);
      loaded.push_back(std::move(task));
    }
  }

  tasks_ = std::move(loaded);
  // Fix hand-edited files: a parent must exist and be top-level.
  for (Task& t : tasks_) {
    const Task* p = find(t.parent);
    if (!p || p->parent != 0 || p->id == t.id) t.parent = 0;
  }
  next_id_ = std::max<TaskId>(max_id + 1,
                              static_cast<TaskId>(root["next_id"].value_or(int64_t{1})));
  active_ = 0;
  set_active(static_cast<TaskId>(root["active"].value_or(int64_t{0})));
  dirty_ = false;
  return true;
}

bool TaskList::save(const std::filesystem::path& path, std::string* error) {
  toml::array list;
  for (const Task& t : tasks_) {
    list.push_back(toml::table{
        {"id", static_cast<int64_t>(t.id)},
        {"parent", static_cast<int64_t>(t.parent)},
        {"title", t.title},
        {"done", t.done},
        {"focus_seconds",
         static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(t.focus).count())},
    });
  }
  toml::table root{
      {"active", static_cast<int64_t>(active_)},
      {"next_id", static_cast<int64_t>(next_id_)},
      {"task", std::move(list)},
  };

  std::ostringstream out;
  out << "# potter tasks — edited by potter, but safe to edit by hand\n\n" << root << '\n';
  if (!write_atomically(path, out.str(), error)) return false;
  dirty_ = false;
  return true;
}

std::filesystem::path tasks_path() {
  return xdg_dir("XDG_DATA_HOME", ".local/share") / "potter" / "tasks.toml";
}

std::string format_focus(std::chrono::nanoseconds amount) {
  using namespace std::chrono;
  if (amount <= amount.zero()) return "";
  const auto total = duration_cast<minutes>(amount).count();
  if (total == 0) return "<1m";
  char buf[32];
  if (total < 60) {
    std::snprintf(buf, sizeof(buf), "%lldm", static_cast<long long>(total));
  } else {
    std::snprintf(buf, sizeof(buf), "%lldh %02lldm", static_cast<long long>(total / 60),
                  static_cast<long long>(total % 60));
  }
  return buf;
}

}  // namespace potter

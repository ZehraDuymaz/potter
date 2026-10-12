#include <algorithm>
#include <string>

#include <ftxui/component/component.hpp>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

TaskPanel::TaskPanel(TaskList& tasks) : tasks_(tasks) {
  InputOption option;
  option.multiline = false;
  option.cursor_position = &cursor_;
  // Plain text with a cursor: no inverted background, so transparency stays.
  option.transform = [](InputState state) {
    if (state.is_placeholder) state.element |= dim;
    return state.element;
  };
  input_ = Input(&buffer_, option);

  const auto rows = tasks_.rows();
  if (!rows.empty()) selected_ = rows.front().task->id;
}

int TaskPanel::selected_index(const std::vector<TaskRow>& rows) const {
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].task->id == selected_) return static_cast<int>(i);
  }
  return rows.empty() ? -1 : 0;
}

void TaskPanel::select_index(const std::vector<TaskRow>& rows, int index) {
  if (rows.empty()) {
    selected_ = 0;
    return;
  }
  index = std::clamp(index, 0, static_cast<int>(rows.size()) - 1);
  selected_ = rows[index].task->id;
}

void TaskPanel::begin_typing(Mode mode, std::string text) {
  mode_ = mode;
  buffer_ = std::move(text);
  cursor_ = static_cast<int>(buffer_.size());
}

void TaskPanel::finish_typing() {
  switch (mode_) {
    case Mode::Add:
      if (TaskId id = tasks_.add(buffer_)) selected_ = id;
      break;
    case Mode::AddSubtask:
      if (TaskId id = tasks_.add(buffer_, selected_)) selected_ = id;
      break;
    case Mode::Rename:
      tasks_.rename(selected_, buffer_);
      break;
    case Mode::Browse:
      break;
  }
  mode_ = Mode::Browse;
  buffer_.clear();
}

bool TaskPanel::handle(const Event& event) {
  if (mode_ != Mode::Browse) {
    if (event == Event::Return) {
      finish_typing();
    } else if (event == Event::Escape) {
      mode_ = Mode::Browse;
      buffer_.clear();
    } else {
      input_->OnEvent(event);
    }
    return true;
  }

  // A delete needs a second 'd'; any other key cancels it.
  const TaskId confirm_delete = pending_delete_;
  pending_delete_ = 0;

  const auto rows = tasks_.rows();
  const int index = selected_index(rows);
  if (index >= 0) selected_ = rows[index].task->id;  // keep selection valid

  if (event == Event::Character('j') || event == Event::ArrowDown) {
    select_index(rows, index + 1);
  } else if (event == Event::Character('k') || event == Event::ArrowUp) {
    select_index(rows, index - 1);
  } else if (event == Event::Character('a')) {
    begin_typing(Mode::Add, "");
  } else if (event == Event::Character('A')) {
    if (selected_ == 0) return true;
    begin_typing(Mode::AddSubtask, "");
  } else if (event == Event::Character('e')) {
    if (const Task* task = tasks_.find(selected_)) begin_typing(Mode::Rename, task->title);
  } else if (event == Event::Character('x')) {
    tasks_.toggle_done(selected_);
  } else if (event == Event::Return) {
    const Task* task = tasks_.find(selected_);
    if (task && !task->done) {
      tasks_.set_active(tasks_.active() == selected_ ? 0 : selected_);
    }
  } else if (event == Event::Character('d')) {
    if (selected_ == 0) return true;
    if (confirm_delete == selected_) {
      tasks_.remove(selected_);
      // Select the row that took the deleted one's place.
      select_index(tasks_.rows(), index);
    } else {
      pending_delete_ = selected_;
    }
  } else if (event == Event::Character('J')) {
    tasks_.move(selected_, +1);
  } else if (event == Event::Character('K')) {
    tasks_.move(selected_, -1);
  } else {
    return false;
  }
  return true;
}

Element TaskPanel::render(Color accent) const {
  const auto rows = tasks_.rows();
  const int index = selected_index(rows);

  Elements lines;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const Task& task = *rows[i].task;
    const bool active = task.id == tasks_.active();
    const std::string time = format_focus(
        rows[i].depth == 0 ? tasks_.total_focus(task.id) : task.focus);

    Element title = text(task.title);
    if (task.done) title = title | dim | strikethrough;
    if (active) title = title | bold | color(accent);

    Element line = hbox({
        text(rows[i].depth == 1 ? "    " : ""),
        text(active ? "▶ " : "  ") | color(accent),
        text(task.done ? "[x] " : "[ ] ") | (task.done ? dim : nothing),
        title | flex,
        text(time.empty() ? "" : " " + time) | dim,
    });
    if (static_cast<int>(i) == index) line = line | inverted | focus;
    lines.push_back(line);
  }

  Element list = rows.empty()
                     ? text("no tasks yet · press a to add one") | dim | center
                     : vbox(std::move(lines)) | vscroll_indicator | yframe;

  Element footer;
  switch (mode_) {
    case Mode::Add:
    case Mode::AddSubtask:
    case Mode::Rename: {
      const char* label = mode_ == Mode::Add          ? "new task: "
                          : mode_ == Mode::AddSubtask ? "new subtask: "
                                                      : "rename: ";
      // FTXUI hides the cursor while the input is empty, so draw our own
      // blinking bar there; once there's text, the input draws it.
      Element field = buffer_.empty() ? text(" ") | focusCursorBarBlinking : input_->Render();
      footer = vbox({
          hbox({text(label) | bold, field | flex}),
          text("enter save · esc cancel") | dim,
      });
      break;
    }
    case Mode::Browse:
      if (pending_delete_ != 0) {
        const Task* task = tasks_.find(pending_delete_);
        const bool has_subtasks = std::any_of(rows.begin(), rows.end(), [&](const TaskRow& r) {
          return r.task->parent == pending_delete_;
        });
        footer = vbox({
            paragraph("delete \"" + (task ? task->title : std::string()) + "\"" +
                      (has_subtasks ? " and its subtasks?" : "?")) |
                color(Color::Red),
            text("press d again · any other key cancels") | dim,
        });
      } else {
        footer = vbox({
            text("a add · A subtask · e rename · x done") | dim,
            text("enter active · dd delete · J/K move") | dim,
        });
      }
      break;
  }

  const std::string count =
      std::to_string(tasks_.count_done()) + "/" + std::to_string(tasks_.size());
  Element content = vbox({
      hbox({text("Tasks") | bold, filler(), text(rows.empty() ? "" : count) | dim}),
      separator(),
      list | flex,
      separator(),
      footer,
  });
  // Breathing room around the panel's content.
  constexpr int kPadX = 2;
  const std::string pad_x(kPadX, ' ');
  return vbox({
      text(""),
      hbox({text(pad_x), content | flex, text(pad_x)}) | flex,
      text(""),
  });
}

}  // namespace potter::ui

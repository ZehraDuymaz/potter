#include <algorithm>
#include <array>
#include <string>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

constexpr int kMinWidth = 60;
constexpr int kMaxWidth = 110;
constexpr int kMinBoxHeight = 7;  // including the border

// The boxes, in reading order: high and medium on top, low and none below.
constexpr std::array<Priority, 4> kOrder{Priority::High, Priority::Medium, Priority::Low,
                                         Priority::None};

const char* box_title(Priority priority) {
  switch (priority) {
    case Priority::High: return "High";
    case Priority::Medium: return "Medium";
    case Priority::Low: return "Low";
    case Priority::None: break;
  }
  return "No priority";
}

}  // namespace

PriorityBoard::PriorityBoard(TaskList& tasks) : tasks_(tasks) {}

void PriorityBoard::open() {
  done_ = false;
  const auto list = open_tasks();
  if (selected_index(list) < 0 && !list.empty()) selected_ = list.front()->id;
}

std::vector<const Task*> PriorityBoard::open_tasks() const {
  std::vector<const Task*> out;
  for (const Priority p : kOrder) {
    for (const TaskRow& row : tasks_.rows()) {
      if (!row.task->done && row.task->priority == p) out.push_back(row.task);
    }
  }
  return out;
}

int PriorityBoard::selected_index(const std::vector<const Task*>& list) const {
  for (std::size_t i = 0; i < list.size(); ++i) {
    if (list[i]->id == selected_) return static_cast<int>(i);
  }
  return -1;
}

bool PriorityBoard::handle(const Event& event) {
  auto list = open_tasks();
  int index = std::max(selected_index(list), 0);
  const auto select = [&](int i) {
    if (list.empty()) {
      selected_ = 0;
      return;
    }
    selected_ = list[std::clamp(i, 0, static_cast<int>(list.size()) - 1)]->id;
  };
  select(index);  // keep the selection on an open task

  if (event == Event::Character('j') || event == Event::ArrowDown) {
    select(index + 1);
  } else if (event == Event::Character('k') || event == Event::ArrowUp) {
    select(index - 1);
  } else if (event == Event::Tab && !list.empty()) {
    // Jump to the first task of the next box that has any.
    const Priority current = list[index]->priority;
    const auto next = std::find_if(list.begin() + index, list.end(),
                                   [&](const Task* t) { return t->priority != current; });
    select(next == list.end() ? 0 : static_cast<int>(next - list.begin()));
  } else if (event == Event::Character('i')) {
    tasks_.cycle_priority(selected_, +1);
  } else if (event == Event::Character('I')) {
    tasks_.cycle_priority(selected_, -1);
  } else if (event == Event::Character('x')) {
    if (tasks_.toggle_done(selected_)) {
      // The task leaves the board; select the one that took its place.
      list = open_tasks();
      select(index);
    }
  } else if (event == Event::Return) {
    if (selected_ != 0) tasks_.set_active(tasks_.active() == selected_ ? 0 : selected_);
  } else if (event == Event::Escape || event == Event::Character('E') ||
             event == Event::Character('q')) {
    done_ = true;
  }
  return true;
}

Element PriorityBoard::render(Color accent) const {
  const int width = screen_width(kMinWidth, kMaxWidth);
  // Two boxes side by side: the width inside the padding, minus the gap
  // between the boxes and each box's own border. (padded_box draws its
  // border outside `width`.)
  const int inner = width - 2 * kPadX;
  const int col = (inner - 1) / 2 - 2;

  const auto list = open_tasks();
  const int index = selected_index(list);

  const auto box = [&](Priority p) {
    Elements lines;
    int count = 0;
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
      const Task& task = *list[i];
      if (task.priority != p) continue;
      ++count;
      const bool active = task.id == tasks_.active();
      const std::string prefix = task.parent != 0 ? "↳ " : "";
      const std::string time = format_focus(task.focus);
      const int title_width = col - 2 - (time.empty() ? 0 : static_cast<int>(time.size()) + 1);
      Element title = text(fit(prefix + task.title, title_width));
      if (active) title = title | bold | color(accent);
      Element line = hbox({
          text(active ? "▶ " : "  ") | color(accent),
          title,
          filler(),
          text(time.empty() ? "" : " " + time) | dim,
      });
      if (i == index) line = line | inverted | focus;
      lines.push_back(line);
    }
    if (lines.empty()) lines.push_back(text("  nothing here") | dim);

    Element heading = hbox({
        text(" "),
        priority_marker(p),
        text(box_title(p)) | bold | color(priority_color(p)),
        text(count > 0 ? " " + std::to_string(count) + " " : " ") | dim,
    });
    // Equal widths, and some height even when empty, so it reads as a grid.
    return window(heading, vbox(std::move(lines)) | vscroll_indicator | yframe | flex) |
           size(WIDTH, EQUAL, col + 2) | size(HEIGHT, GREATER_THAN, kMinBoxHeight);
  };

  Element grid = vbox({
      hbox({box(kOrder[0]), filler(), box(kOrder[1])}) | flex,
      hbox({box(kOrder[2]), filler(), box(kOrder[3])}) | flex,
  });

  const int done = tasks_.count_done();
  const std::string summary = std::to_string(list.size()) + " open" +
                              (done > 0 ? " - " + std::to_string(done) + " done" : "");
  const auto hints = wrap_hints(
      {"↑↓/jk move", "tab next box", "i/I priority", "x done", "enter active", "esc close"}, inner);
  Elements footer;
  for (const std::string& line : hints) footer.push_back(text(line) | dim);

  Element content = vbox({
      hbox({text("Priorities") | bold, filler(), text(summary) | dim}),
      separator(),
      grid | flex,
      separator(),
      vbox(std::move(footer)),
  });
  return padded_box(std::move(content), width) | size(HEIGHT, LESS_THAN, 40);
}

}  // namespace potter::ui

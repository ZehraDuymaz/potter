#include "app.hpp"

#include <iostream>

#include <ftxui/screen/terminal.hpp>

#include "notify.hpp"

using namespace ftxui;

namespace potter {

namespace {

// Focus time is saved at least this often, so a crash loses little.
constexpr auto kSaveInterval = std::chrono::seconds(30);

// Below this width the task panel would squash the timer, so it's hidden.
constexpr int kMinWidthForTasks = 82;

}  // namespace

App::App(Config config, std::filesystem::path config_path, std::filesystem::path tasks_path,
         std::function<void()> quit)
    : config_(std::move(config)),
      config_path_(std::move(config_path)),
      quit_(std::move(quit)),
      timer_(config_.timer),
      tasks_path_(std::move(tasks_path)) {
  std::string error;
  if (!tasks_.load(tasks_path_, &error)) status_ = error;
  last_save_ = Timer::Clock::now();
}

bool App::on_event(const Event& event) {
  if (event == Event::Custom) return false;  // the redraw tick

  const auto now = Timer::Clock::now();
  // Credit focus time to the current task before anything can change it.
  collect_focus(now);

  // Handle Ctrl-C ourselves so tasks get saved on the way out.
  if (event == Event::CtrlC) {
    quit_();
    return true;
  }

  switch (view_) {
    case View::Settings:
      settings_.handle(event);
      if (settings_.done()) close_settings();
      return true;

    case View::Help:
      if (event.is_character() || event == Event::Escape || event == Event::Return) {
        view_ = View::Timer;
        return true;
      }
      return false;

    case View::Timer:
      break;
  }

  if (tasks_visible() && task_panel_.handle(event)) {
    if (tasks_.dirty()) save_tasks(now);
    return true;
  }

  if (event == Event::Character(' ')) {
    timer_.toggle(now);
  } else if (event == Event::Character('r')) {
    timer_.reset();
  } else if (event == Event::Character('n')) {
    timer_.skip();
  } else if (event == Event::Character('t')) {
    config_.show_tasks = !config_.show_tasks;
    save_config(config_, config_path_);
    if (config_.show_tasks && !tasks_visible()) {
      set_status("make the window wider to see tasks");
      return true;
    }
  } else if (event == Event::Character('s')) {
    settings_.open(config_);
    view_ = View::Settings;
  } else if (event == Event::Character('?')) {
    view_ = View::Help;
  } else if (event == Event::Character('q')) {
    quit_();
  } else {
    return false;
  }
  status_.clear();
  return true;
}

Element App::render() {
  const auto now = Timer::Clock::now();
  const auto term = Terminal::Size();
  const bool show_tasks = tasks_visible() && view_ == View::Timer;
  const int timer_width = term.dimx - (show_tasks ? ui::TaskPanel::kWidth + 1 : 0);
  const bool big = timer_width >= 50 && term.dimy >= 14;

  Element body;
  switch (view_) {
    case View::Timer:
      body = ui::timer_view(timer_, config_, now, big, active_task_line());
      if (show_tasks) {
        body = hbox({
            body | flex,
            separator(),
            task_panel_.render(ui::phase_color(Phase::Focus, config_)) |
                size(WIDTH, EQUAL, ui::TaskPanel::kWidth),
        });
      }
      break;
    case View::Settings: body = settings_.render(); break;
    case View::Help: body = ui::help_view(); break;
  }
  if (status_.empty()) return body;
  return vbox({body | flex, text(status_) | color(Color::Red) | hcenter});
}

void App::tick() {
  const auto now = Timer::Clock::now();
  collect_focus(now);  // up to the end of the phase, before it moves on
  const auto finished = timer_.update(now);
  if (finished) on_phase_finished(*finished);

  if (tasks_.dirty() && (finished || now - last_save_ >= kSaveInterval)) save_tasks(now);
}

void App::shutdown() {
  const auto now = Timer::Clock::now();
  collect_focus(now);
  if (tasks_.dirty()) save_tasks(now);
}

void App::collect_focus(Timer::Clock::time_point now) {
  // Always measure, so time with no active task isn't credited later.
  const auto amount = focus_meter_.collect(timer_, now);
  if (tasks_.active() != 0) tasks_.add_focus(tasks_.active(), amount);
}

void App::save_tasks(Timer::Clock::time_point now) {
  last_save_ = now;
  std::string error;
  if (!tasks_.save(tasks_path_, &error)) status_ = error;
}

bool App::tasks_visible() const {
  return config_.show_tasks && Terminal::Size().dimx >= kMinWidthForTasks;
}

std::string App::active_task_line() const {
  const Task* task = tasks_.find(tasks_.active());
  if (!task) return "";
  std::string line = "▶ " + task->title;
  if (const std::string time = format_focus(task->focus); !time.empty()) {
    line += " · " + time;
  }
  return line;
}

void App::on_phase_finished(Phase finished) {
  if (config_.terminal_bell) std::cout << '\a' << std::flush;
  if (!config_.desktop_notifications) return;

  if (finished == Phase::Focus) {
    send_notification("Focus done 🍅", timer_.phase() == Phase::LongBreak
                                          ? "Nice work! Time for a long break."
                                          : "Time for a short break.");
  } else {
    send_notification("Break over", "Back to focus.");
  }
}

void App::close_settings() {
  config_ = settings_.draft();
  timer_.apply_settings(config_.timer);
  view_ = View::Timer;

  std::string error;
  if (!save_config(config_, config_path_, &error)) status_ = error;
}

}  // namespace potter

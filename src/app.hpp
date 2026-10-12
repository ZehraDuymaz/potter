#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "config.hpp"
#include "tasks.hpp"
#include "timer.hpp"
#include "ui/views.hpp"

namespace potter {

// Ties the timer, config and views together and routes key presses.
class App {
 public:
  App(Config config, std::filesystem::path config_path, std::filesystem::path tasks_path,
      std::function<void()> quit);

  bool on_event(const ftxui::Event& event);
  ftxui::Element render();
  // Called a few times per second from the main loop.
  void tick();
  // Call once after the main loop ends: credits the last focus time and saves.
  void shutdown();

  // A message shown at the bottom, e.g. a config error.
  void set_status(std::string status) { status_ = std::move(status); }

 private:
  enum class View { Timer, Settings, Help };

  void on_phase_finished(Phase finished);
  void close_settings();
  // Credits focus time that passed since the last call to the active task.
  void collect_focus(Timer::Clock::time_point now);
  void save_tasks(Timer::Clock::time_point now);
  std::string active_task_line() const;
  // The task panel is shown when enabled and the window is wide enough.
  bool tasks_visible() const;

  Config config_;
  std::filesystem::path config_path_;
  std::function<void()> quit_;
  Timer timer_;
  FocusMeter focus_meter_;
  TaskList tasks_;
  std::filesystem::path tasks_path_;
  Timer::Clock::time_point last_save_{};
  ui::SettingsMenu settings_;
  ui::TaskPanel task_panel_{tasks_};
  View view_ = View::Timer;
  std::string status_;
};

}  // namespace potter

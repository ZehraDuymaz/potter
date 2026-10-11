#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "config.hpp"
#include "timer.hpp"
#include "ui/views.hpp"

namespace potter {

// Ties the timer, config and views together and routes key presses.
class App {
 public:
  App(Config config, std::filesystem::path config_path, std::function<void()> quit);

  bool on_event(const ftxui::Event& event);
  ftxui::Element render();
  // Called a few times per second from the main loop.
  void tick();

  // A message shown at the bottom, e.g. a config error.
  void set_status(std::string status) { status_ = std::move(status); }

 private:
  enum class View { Timer, Settings, Help };

  void on_phase_finished(Phase finished);
  void close_settings();

  Config config_;
  std::filesystem::path config_path_;
  std::function<void()> quit_;
  Timer timer_;
  ui::SettingsMenu settings_;
  View view_ = View::Timer;
  std::string status_;
};

}  // namespace potter

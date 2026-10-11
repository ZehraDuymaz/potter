#pragma once

#include <string>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "config.hpp"
#include "timer.hpp"

namespace potter::ui {

ftxui::Color phase_color(Phase phase, const Config& config);

ftxui::Element timer_view(const Timer& timer, const Config& config,
                          Timer::Clock::time_point now);

ftxui::Element help_view();

// The settings screen. It edits a draft copy of the config; the app reads
// the draft back once the menu is closed.
class SettingsMenu {
 public:
  void open(const Config& config);
  // Handles a key. Always consumes it, since the menu is modal.
  bool handle(const ftxui::Event& event);
  ftxui::Element render() const;

  bool done() const { return done_; }
  const Config& draft() const { return draft_; }

 private:
  void adjust(int delta);
  void activate();

  Config draft_;
  int selected_ = 0;
  bool done_ = false;
  bool editing_ = false;
  std::string edit_buffer_;
  std::string error_;
};

}  // namespace potter::ui

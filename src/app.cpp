#include "app.hpp"

#include <iostream>

#include "notify.hpp"

using namespace ftxui;

namespace potter {

App::App(Config config, std::filesystem::path config_path, std::function<void()> quit)
    : config_(std::move(config)),
      config_path_(std::move(config_path)),
      quit_(std::move(quit)),
      timer_(config_.timer) {}

bool App::on_event(const Event& event) {
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

  if (event == Event::Character(' ')) {
    timer_.toggle(Timer::Clock::now());
  } else if (event == Event::Character('r')) {
    timer_.reset();
  } else if (event == Event::Character('n')) {
    timer_.skip();
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
  Element body;
  switch (view_) {
    case View::Timer: body = ui::timer_view(timer_, config_, Timer::Clock::now()); break;
    case View::Settings: body = settings_.render(); break;
    case View::Help: body = ui::help_view(); break;
  }
  if (status_.empty()) return body;
  return vbox({body | flex, text(status_) | color(Color::Red) | hcenter});
}

void App::tick() {
  if (auto finished = timer_.update(Timer::Clock::now())) on_phase_finished(*finished);
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

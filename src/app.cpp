#include "app.hpp"

#include <iostream>

#include <ftxui/screen/terminal.hpp>

#include "system/notify.hpp"

using namespace ftxui;

namespace potter {

namespace {

// Focus time is saved at least this often, so a crash loses little.
constexpr auto kSaveInterval = std::chrono::seconds(30);

// How far ←/→ jumps in a track, in seconds.
constexpr double kSeekStep = 10;

// Below this width the task panel would squash the timer, so it's hidden.
constexpr int kMinWidthForTasks = 82;

}  // namespace

App::App(Config config, Paths paths, std::function<void()> quit)
    : config_(std::move(config)),
      paths_(std::move(paths)),
      quit_(std::move(quit)),
      timer_(config_.timer),
      player_(config_.music_volume, config_.mpv_args) {
  std::string error;
  if (!tasks_.load(paths_.tasks, &error)) status_ = error;
  if (!history_.load(paths_.history, &error)) status_ = error;
  if (!favorites_.load(paths_.favorites, &error)) status_ = error;
  if (!resume_points_.load(paths_.positions, &error)) status_ = error;
  last_save_ = Timer::Clock::now();
}

bool App::on_event(const Event& event) {
  if (event == Event::Custom) return false;  // the redraw tick

  const auto now = Timer::Clock::now();
  // Credit focus time to the current task before anything can change it.
  collect_focus(now);

  // Handle Ctrl-C ourselves so everything gets saved on the way out.
  if (event == Event::CtrlC) {
    quit_();
    return true;
  }

  switch (view_) {
    case View::Settings:
      settings_.handle(event);
      if (settings_.done()) close_settings();
      return true;

    case View::History:
      history_screen_.handle(event, today());
      if (history_.dirty()) save_data(now);
      if (history_screen_.done()) view_ = View::Timer;
      return true;

    case View::Help:
      if (event.is_character() || event == Event::Escape || event == Event::Return) {
        view_ = View::Timer;
        return true;
      }
      return false;

    case View::Music: {
      // Pausing or playing by hand overrides the automatic break pause.
      if (event == Event::Character('p') || event == Event::Character(' ') ||
          event == Event::Return) {
        paused_for_break_ = false;
      }
      music_screen_.handle(event);
      if (player_.volume() != config_.music_volume) {
        config_.music_volume = player_.volume();
        config_dirty_ = true;
      }
      if (favorites_.dirty()) save_data(now);
      if (music_screen_.done()) view_ = View::Timer;
      return true;
    }

    case View::Timer:
      break;
  }

  if (tasks_visible() && task_panel_.handle(event)) {
    if (data_dirty()) save_data(now);
    return true;
  }
  if (handle_seek_key(event)) return true;

  if (event == Event::Character(' ')) {
    timer_.toggle(now);
  } else if (event == Event::Character('r')) {
    timer_.reset();
  } else if (event == Event::Character('n')) {
    timer_.skip();
    on_phase_changed();
  } else if (event == Event::Character('t')) {
    config_.show_tasks = !config_.show_tasks;
    save_config(config_, paths_.config);
    if (config_.show_tasks && !tasks_visible()) {
      set_status("make the window wider to see tasks");
      return true;
    }
  } else if (event == Event::Character('m')) {
    music_screen_.open();
    view_ = View::Music;
  } else if (event == Event::Character('p')) {
    if (player_.state().has_media()) {
      player_.toggle_pause();
      paused_for_break_ = false;
    } else {
      set_status("nothing playing · press m for music");
      return true;
    }
  } else if (event == Event::Character('+') || event == Event::Character('=')) {
    change_volume(+5);
  } else if (event == Event::Character('-')) {
    change_volume(-5);
  } else if (event == Event::Character('s')) {
    settings_.open(config_);
    view_ = View::Settings;
  } else if (event == Event::Character('h')) {
    history_screen_.open();
    view_ = View::History;
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
  const Color accent = ui::phase_color(Phase::Focus, config_);

  Element body;
  switch (view_) {
    case View::Timer:
      body = ui::timer_view(timer_, config_, now, big, active_task_line(), today_line(),
                            music_line(), music_time_line(), music_progress());
      if (show_tasks) {
        body = hbox({
            body | flex,
            separator(),
            task_panel_.render(accent) | size(WIDTH, EQUAL, ui::TaskPanel::kWidth),
        });
      }
      break;
    case View::Settings: body = settings_.render(); break;
    case View::Help: body = ui::help_view(); break;
    case View::History: body = history_screen_.render(today()); break;
    case View::Music: body = music_screen_.render(accent); break;
  }
  if (status_.empty()) return body;
  return vbox({body | flex, text(status_) | color(Color::Red) | hcenter});
}

void App::tick() {
  const auto now = Timer::Clock::now();
  collect_focus(now);  // up to the end of the phase, before it moves on
  const auto finished = timer_.update(now);
  if (finished) {
    on_phase_finished(*finished);
    on_phase_changed();
  }

  record_position();
  const bool dirty = data_dirty() || resume_points_.dirty();
  if (dirty && (finished || now - last_save_ >= kSaveInterval)) save_data(now);
}

void App::shutdown() {
  const auto now = Timer::Clock::now();
  collect_focus(now);
  record_position();
  if (data_dirty() || resume_points_.dirty()) save_data(now);
  if (config_dirty_) save_config(config_, paths_.config);
  player_.stop();  // music doesn't outlive potter
}

void App::collect_focus(Timer::Clock::time_point now) {
  // Always measure, so time with no active task isn't credited later.
  const auto amount = focus_meter_.collect(timer_, now);
  if (amount <= amount.zero()) return;
  if (tasks_.active() != 0) tasks_.add_focus(tasks_.active(), amount);
  const Task* active = tasks_.find(tasks_.active());
  if (history_.add_focus(today(), amount, config_.daily_goal_minutes, active ? active->id : 0,
                         active ? active->title : "")) {
    on_goal_reached();
  }
}

void App::save_data(Timer::Clock::time_point now) {
  last_save_ = now;
  std::string error;
  if (tasks_.dirty() && !tasks_.save(paths_.tasks, &error)) status_ = error;
  if (history_.dirty() && !history_.save(paths_.history, &error)) status_ = error;
  if (favorites_.dirty() && !favorites_.save(paths_.favorites, &error)) status_ = error;
  if (resume_points_.dirty() && !resume_points_.save(paths_.positions, &error)) {
    status_ = error;
  }
}

void App::record_position() {
  // While a new file loads, mpv may still report the old one's position.
  if (player_.loading()) return;
  const PlayerState state = player_.state();
  if (state.has_media() && state.time_pos && !state.path.empty()) {
    resume_points_.record(state.path, *state.time_pos, state.duration);
  }
}

bool App::handle_seek_key(const Event& event) {
  const bool arrow = event == Event::ArrowLeft || event == Event::ArrowRight;
  const bool digit = event.is_character() && event.character().size() == 1 &&
                     event.character()[0] >= '0' && event.character()[0] <= '9';
  if (!arrow && !digit) return false;

  if (!player_.state().has_media() && !player_.loading()) {
    set_status("nothing playing · press m for music");
    return true;
  }
  if (event == Event::ArrowLeft) {
    player_.seek_by(-kSeekStep);
  } else if (event == Event::ArrowRight) {
    player_.seek_by(kSeekStep);
  } else {
    player_.seek_percent((event.character()[0] - '0') * 10);
  }
  status_.clear();
  return true;
}

void App::change_volume(int delta) {
  config_.music_volume = player_.change_volume(delta);
  config_dirty_ = true;
}

void App::on_phase_changed() {
  if (!config_.pause_music_on_breaks) return;
  if (timer_.phase() != Phase::Focus) {
    if (player_.state().playing()) {
      player_.set_pause(true);
      paused_for_break_ = true;
    }
  } else if (paused_for_break_) {
    player_.set_pause(false);
    paused_for_break_ = false;
  }
}

Day App::today() const {
  return day_of(std::chrono::system_clock::now(), config_.day_start_hour);
}

std::string App::today_line() const {
  const DayStats stats = history_.get(today());
  std::string focus = format_focus(stats.focus);
  if (focus.empty()) focus = "0m";
  std::string line =
      focus + " / " + format_focus(std::chrono::minutes(config_.daily_goal_minutes)) + " today";
  if (stats.goal_met()) line += " ✓";
  if (const int streak = history_.streak(today()); streak > 0) {
    line += " · 🔥 " + std::to_string(streak);
  }
  return line;
}

std::string App::music_line() const {
  if (player_.state().has_media()) return "♪ " + player_.display_title();
  if (player_.loading()) return "♪ loading " + player_.display_title() + "…";
  return "";
}

std::string App::music_time_line() const {
  const PlayerState state = player_.state();
  if (!state.has_media()) return "";
  std::string line = (state.paused ? "▶ " : "⏸ ") + format_clock(state.time_pos.value_or(0));
  if (state.duration && *state.duration > 0) line += " / " + format_clock(*state.duration);
  if (paused_for_break_) line += " · paused for the break";
  return line;
}

std::optional<float> App::music_progress() const {
  const PlayerState state = player_.state();
  if (!state.has_media() || !state.duration || *state.duration <= 0) return std::nullopt;
  return static_cast<float>(state.time_pos.value_or(0) / *state.duration);
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
  if (finished == Phase::Focus) history_.add_session(today(), config_.daily_goal_minutes);

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

void App::on_goal_reached() {
  if (config_.terminal_bell) std::cout << '\a' << std::flush;
  if (!config_.desktop_notifications) return;
  send_notification(
      "Daily goal reached 🎉",
      format_focus(std::chrono::minutes(config_.daily_goal_minutes)) + " of focus today. Nice!");
}

void App::close_settings() {
  config_ = settings_.draft();
  timer_.apply_settings(config_.timer);
  // A changed goal applies to today too; past days keep their own goal.
  history_.set_goal(today(), config_.daily_goal_minutes);
  view_ = View::Timer;

  std::string error;
  if (!save_config(config_, paths_.config, &error)) status_ = error;
  config_dirty_ = false;
}

}  // namespace potter

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "core/config.hpp"
#include "core/history.hpp"
#include "core/music.hpp"
#include "system/player.hpp"
#include "system/search.hpp"
#include "core/tasks.hpp"
#include "core/timer.hpp"
#include "ui/views.hpp"

namespace potter {

// Where potter keeps its files.
struct Paths {
  std::filesystem::path config;
  std::filesystem::path tasks;
  std::filesystem::path history;
  std::filesystem::path favorites;
  std::filesystem::path positions;
};

// Ties the timer, config and views together and routes key presses.
class App {
 public:
  App(Config config, Paths paths, std::function<void()> quit);

  bool on_event(const ftxui::Event& event);
  ftxui::Element render();
  // Called a few times per second from the main loop.
  void tick();
  // Call once after the main loop ends: credits the last focus time, saves,
  // and stops the music.
  void shutdown();

  // A message shown at the bottom, e.g. a config error.
  void set_status(std::string status) { status_ = std::move(status); }

 private:
  enum class View { Timer, Settings, Help, History, Music };

  void on_phase_finished(Phase finished);
  // Pauses music when a break begins and resumes it when focus begins again.
  void on_phase_changed();
  void on_goal_reached();
  void close_settings();
  // Credits focus time that passed since the last call to the active task
  // and to today's history.
  void collect_focus(Timer::Clock::time_point now);
  // Saves tasks, history, favorites and track positions if they changed.
  void save_data(Timer::Clock::time_point now);
  bool data_dirty() const { return tasks_.dirty() || history_.dirty() || favorites_.dirty(); }
  void change_volume(int delta);
  // Notes where the playing track is, so it can be resumed later.
  void record_position();
  // ←/→ and 0-9 on the timer screen. Returns false for other keys.
  bool handle_seek_key(const ftxui::Event& event);
  // The current day, according to the day-start setting.
  Day today() const;
  std::string active_task_line() const;
  std::string today_line() const;
  // What's playing, and where in it (under the timer).
  std::string music_line() const;
  std::string music_time_line() const;
  // How far into the track music is, for the bar under the music line.
  std::optional<float> music_progress() const;
  // The task panel is shown when enabled and the window is wide enough.
  bool tasks_visible() const;

  Config config_;
  Paths paths_;
  std::function<void()> quit_;
  Timer timer_;
  FocusMeter focus_meter_;
  TaskList tasks_;
  History history_;
  Favorites favorites_;
  ResumePoints resume_points_;
  MpvPlayer player_;
  YouTubeSearch search_;
  bool paused_for_break_ = false;  // music was paused by a break, not by you
  bool config_dirty_ = false;      // e.g. volume changed; saved on quit
  Timer::Clock::time_point last_save_{};
  ui::SettingsMenu settings_;
  ui::TaskPanel task_panel_{tasks_};
  ui::MusicScreen music_screen_{player_, search_, favorites_, resume_points_, config_};
  ui::HistoryScreen history_screen_{history_, tasks_, config_};
  View view_ = View::Timer;
  std::string status_;
};

}  // namespace potter

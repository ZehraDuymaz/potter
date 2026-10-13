#pragma once

#include <string>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "core/config.hpp"
#include "core/history.hpp"
#include "core/music.hpp"
#include "core/tasks.hpp"
#include "core/timer.hpp"

namespace potter {
class MpvPlayer;
class YouTubeSearch;
}  // namespace potter

namespace potter::ui {

ftxui::Color phase_color(Phase phase, const Config& config);

// `big` draws the countdown in large digits; `active_task` and `music` are
// lines shown under the timer (empty for none); `today` is goal progress.
ftxui::Element timer_view(const Timer& timer, const Config& config,
                          Timer::Clock::time_point now, bool big,
                          const std::string& active_task, const std::string& today,
                          const std::string& music);

ftxui::Element help_view();

// Vertical bar chart of the last two weeks, with the goal line and streaks.
ftxui::Element history_view(const History& history, Day today, const Config& config);

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

// The task list panel next to the timer.
class TaskPanel {
 public:
  explicit TaskPanel(TaskList& tasks);
  TaskPanel(const TaskPanel&) = delete;  // the text input points into this object
  TaskPanel& operator=(const TaskPanel&) = delete;

  // Handles a key. Returns false for keys the panel doesn't use, except while
  // typing a title, when it takes every key.
  bool handle(const ftxui::Event& event);
  ftxui::Element render(ftxui::Color accent) const;

  static constexpr int kWidth = 50;  // including padding

 private:
  enum class Mode { Browse, Add, AddSubtask, Rename };

  void begin_typing(Mode mode, std::string text);
  void finish_typing();
  int selected_index(const std::vector<TaskRow>& rows) const;
  void select_index(const std::vector<TaskRow>& rows, int index);

  TaskList& tasks_;
  TaskId selected_ = 0;
  TaskId pending_delete_ = 0;
  Mode mode_ = Mode::Browse;
  std::string buffer_;
  int cursor_ = 0;
  ftxui::Component input_;
  // Wraps input_ so the typing cursor wins over the list's selected row.
  ftxui::Component field_;
};

// The music screen: now playing, a search/link box, results and favorites.
class MusicScreen {
 public:
  MusicScreen(MpvPlayer& player, YouTubeSearch& search, Favorites& favorites,
              const Config& config);
  MusicScreen(const MusicScreen&) = delete;  // the text input points into this object
  MusicScreen& operator=(const MusicScreen&) = delete;

  void open();
  // Handles a key. Always consumes it, since the screen is modal.
  bool handle(const ftxui::Event& event);
  bool done() const { return done_; }
  ftxui::Element render(ftxui::Color accent) const;

 private:
  // A selectable row: a search result or a favorite.
  struct Item {
    bool is_result;
    std::size_t index;
  };
  std::vector<Item> items() const;
  void submit();
  void play_item(const Item& item);
  void toggle_favorite();

  MpvPlayer& player_;
  YouTubeSearch& search_;
  Favorites& favorites_;
  const Config& config_;

  bool typing_ = false;
  bool done_ = false;
  int selected_ = 0;
  int pending_delete_ = -1;  // favorite index waiting for a second 'd'
  std::string message_;
  std::string buffer_;
  int cursor_ = 0;
  ftxui::Component input_;
  ftxui::Component field_;
};

}  // namespace potter::ui

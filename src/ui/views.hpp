#pragma once

#include <optional>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "core/config.hpp"
#include "core/history.hpp"
#include "core/music.hpp"
#include "core/sounds.hpp"
#include "core/tasks.hpp"
#include "core/timer.hpp"

namespace potter {
class MpvPlayer;
class YouTubeSearch;
class SoundBoard;
}  // namespace potter

namespace potter::ui {

ftxui::Color phase_color(Phase phase, const Config& config);

// `big` draws the countdown in large digits; `active_task`, `music` (the
// title) and `music_time` are lines shown under the timer (empty for none);
// `today` is goal progress; `music_progress` (0-1) draws a bar for the track;
// `sounds` lists the background sounds that are on.
ftxui::Element timer_view(const Timer& timer, const Config& config,
                          Timer::Clock::time_point now, bool big,
                          const std::string& active_task, const std::string& today,
                          const std::string& music, const std::string& music_time,
                          std::optional<float> music_progress, const std::string& sounds);

ftxui::Element help_view();

// Shared text and layout helpers (ui/common.cpp).
inline constexpr int kPadX = 2;  // spaces inside a screen's border, left and right
// Cuts `s` to at most `width` terminal columns, ending with "…" if cut.
std::string fit(const std::string& s, int width);
// Pads `s` on the left to `width` terminal columns.
std::string pad_left(const std::string& s, int width);
// A screen width that grows with the terminal, within limits.
int screen_width(int min_width, int max_width);
// Wraps a screen's content in padding and a rounded border, centered.
ftxui::Element padded_box(ftxui::Element content, int width);

// The history screen. "Days": a bar chart of the last two weeks; ←/→ picks a
// day and shows where its focus went. "Tasks": focus totals per task over
// 7 days, 30 days or all time — including finished and deleted tasks — and
// `dd` removes a task's time from the history (e.g. test runs).
class HistoryScreen {
 public:
  HistoryScreen(History& history, const TaskList& tasks, const Config& config);

  void open();
  // Handles a key. Always consumes it, since the screen is modal.
  bool handle(const ftxui::Event& event, Day today);
  bool done() const { return done_; }
  ftxui::Element render(Day today) const;

 private:
  enum class Tab { Days, Tasks };
  enum class Range { Week, Month, All };

  ftxui::Element render_chart(Day today) const;
  ftxui::Element render_day(Day day, Day today, int inner) const;
  ftxui::Element render_totals(Day today, int inner) const;
  ftxui::Element task_row(const TaskTime& entry, int inner) const;
  // Totals for the current range; `range_label` gets e.g. "last 7 days".
  std::vector<TaskTime> totals(Day today, std::string* range_label = nullptr) const;
  // The name to show for a history entry (current title if the task exists).
  std::string display_title(const TaskTime& entry) const;

  History& history_;
  const TaskList& tasks_;
  const Config& config_;

  Tab tab_ = Tab::Days;
  Range range_ = Range::Week;
  int day_offset_ = 0;  // 0 = today, 1 = yesterday, ...
  int scroll_ = 0;      // selected row in the Tasks tab
  std::optional<TaskId> pending_remove_;  // waiting for the second 'd'
  std::string message_;                   // e.g. "removed … from your history"
  bool done_ = false;
};

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
              const ResumePoints& resume_points, const Config& config);
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
  // Plays a track and, if you left it halfway before, offers to resume.
  void play(const std::string& target, const std::string& title);
  // The offer to resume, while its track is the one playing.
  std::optional<int> resume_offer() const;
  void toggle_favorite();

  MpvPlayer& player_;
  YouTubeSearch& search_;
  Favorites& favorites_;
  const ResumePoints& resume_points_;
  const Config& config_;

  bool typing_ = false;
  bool done_ = false;
  int selected_ = 0;
  int pending_delete_ = -1;  // favorite index waiting for a second 'd'
  std::string offer_target_;   // track that can be resumed with r
  int offer_seconds_ = 0;      // where it resumes
  std::string message_;
  std::string buffer_;
  int cursor_ = 0;
  ftxui::Component input_;
  ftxui::Component field_;
};

// The sounds screen: background sounds like rain or brown noise, each with
// its own volume, mixed together.
class SoundsScreen {
 public:
  SoundsScreen(SoundBoard& board, SoundMix& mix, std::filesystem::path dir);

  void open();
  // Handles a key. Always consumes it, since the screen is modal.
  bool handle(const ftxui::Event& event);
  bool done() const { return done_; }
  ftxui::Element render(ftxui::Color accent) const;

 private:
  void toggle(const Sound& sound);
  void change_volume(const Sound& sound, int delta);
  void bring_back_last_mix();

  SoundBoard& board_;
  SoundMix& mix_;
  std::filesystem::path dir_;
  std::vector<Sound> sounds_;
  int selected_ = 0;
  bool done_ = false;
  std::string message_;
};

}  // namespace potter::ui

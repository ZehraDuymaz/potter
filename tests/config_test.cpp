// Unit tests for config.toml loading and saving. Same style as timer_test.cpp.
//
// The config is a file people edit by hand and that older potter versions
// wrote, so most of these check that a strange file never crashes potter or
// loses settings: missing keys, values out of range, wrong types, bad TOML.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/config.hpp"

using namespace potter;
using namespace std::chrono_literals;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static const auto kDir = std::filesystem::temp_directory_path() / "potter-config-test";

// Writes `text` as a config file and loads it.
static Config load_text(const std::string& text, std::string* error = nullptr) {
  std::filesystem::create_directories(kDir);
  const auto path = kDir / "config.toml";
  std::ofstream(path) << text;
  return load_config(path, error);
}

static std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

static void parses_hex_colors() {
  const auto c = parse_hex_color("#e06c75");
  CHECK(c && c->r == 0xe0 && c->g == 0x6c && c->b == 0x75);
  CHECK(parse_hex_color("E06C75"));  // '#' is optional, uppercase works
  CHECK(!parse_hex_color(""));
  CHECK(!parse_hex_color("#"));
  CHECK(!parse_hex_color("#fff"));      // short form isn't supported
  CHECK(!parse_hex_color("#e06c75ff"));  // neither is alpha
  CHECK(!parse_hex_color("#e06c7g"));
  CHECK(!parse_hex_color("red"));
}

static void first_run_writes_defaults() {
  std::filesystem::remove_all(kDir);
  const auto path = kDir / "sub" / "config.toml";  // parent doesn't exist yet

  std::string error;
  const Config config = load_config(path, &error);
  CHECK(error.empty());
  CHECK(std::filesystem::exists(path));
  CHECK(config.timer.focus == 25min);
  CHECK(config.music_volume == 70);

  // The file it wrote loads back to the same defaults.
  const Config again = load_config(path, &error);
  CHECK(error.empty());
  CHECK(again.timer.focus == 25min);
  CHECK(again.daily_goal_minutes == 100);
  CHECK(again.theme == ThemeMode::Terminal);
}

static void saves_and_loads_every_setting() {
  std::filesystem::remove_all(kDir);
  const auto path = kDir / "config.toml";

  Config config;
  config.timer.focus = 50min;
  config.timer.short_break = 10min;
  config.timer.long_break = 30min;
  config.timer.sessions_before_long_break = 3;
  config.timer.auto_start_breaks = true;
  config.timer.auto_start_focus = true;
  config.desktop_notifications = false;
  config.terminal_bell = false;
  config.theme = ThemeMode::Custom;
  config.focus_color = "#112233";
  config.short_break_color = "#445566";
  config.long_break_color = "#778899";
  config.show_tasks = false;
  config.task_priorities = false;
  config.sort_tasks_by_priority = true;
  config.daily_goal_minutes = 240;
  config.day_start_hour = 6;
  config.music_volume = 35;
  config.pause_music_on_breaks = false;
  config.music_search_results = 12;
  config.mpv_args = "--audio-device=pipewire \"--title=a b\"";

  std::string error;
  CHECK(save_config(config, path, &error));
  CHECK(error.empty());

  const Config loaded = load_config(path, &error);
  CHECK(error.empty());
  CHECK(loaded.timer.focus == 50min);
  CHECK(loaded.timer.short_break == 10min);
  CHECK(loaded.timer.long_break == 30min);
  CHECK(loaded.timer.sessions_before_long_break == 3);
  CHECK(loaded.timer.auto_start_breaks);
  CHECK(loaded.timer.auto_start_focus);
  CHECK(!loaded.desktop_notifications);
  CHECK(!loaded.terminal_bell);
  CHECK(loaded.theme == ThemeMode::Custom);
  CHECK(loaded.focus_color == "#112233");
  CHECK(loaded.short_break_color == "#445566");
  CHECK(loaded.long_break_color == "#778899");
  CHECK(!loaded.show_tasks);
  CHECK(!loaded.task_priorities);
  CHECK(loaded.sort_tasks_by_priority);
  CHECK(loaded.daily_goal_minutes == 240);
  CHECK(loaded.day_start_hour == 6);
  CHECK(loaded.music_volume == 35);
  CHECK(!loaded.pause_music_on_breaks);
  CHECK(loaded.music_search_results == 12);
  CHECK(loaded.mpv_args == config.mpv_args);  // quotes survive
}

// A config from an older potter only has some sections. Everything it
// doesn't mention keeps its default, and unknown sections are ignored.
static void missing_keys_keep_defaults() {
  std::string error;
  const Config config = load_text(R"(
[timer]
focus_minutes = 45

[future_feature]
something = true
)",
                                  &error);
  CHECK(error.empty());
  CHECK(config.timer.focus == 45min);
  CHECK(config.timer.short_break == 5min);
  CHECK(config.timer.sessions_before_long_break == 4);
  CHECK(config.desktop_notifications);
  CHECK(config.theme == ThemeMode::Terminal);
  CHECK(config.daily_goal_minutes == 100);
  CHECK(config.music_volume == 70);
  CHECK(config.mpv_args.empty());
  CHECK(config.task_priorities);  // on by default
  CHECK(!config.sort_tasks_by_priority);

  // An empty file is just a config with nothing set.
  const Config empty = load_text("", &error);
  CHECK(error.empty());
  CHECK(empty.timer.focus == 25min);
}

static void clamps_values_out_of_range() {
  const Config low = load_text(R"(
[timer]
focus_minutes = 0
short_break_minutes = -5
long_break_minutes = 0
sessions_before_long_break = 0
[goals]
daily_minutes = 1
day_start_hour = -3
[music]
volume = -20
search_results = 0
)");
  CHECK(low.timer.focus == 1min);
  CHECK(low.timer.short_break == 1min);
  CHECK(low.timer.long_break == 1min);
  CHECK(low.timer.sessions_before_long_break == 1);
  CHECK(low.daily_goal_minutes == 5);
  CHECK(low.day_start_hour == 0);
  CHECK(low.music_volume == 0);
  CHECK(low.music_search_results == 1);

  const Config high = load_text(R"(
[timer]
focus_minutes = 9999
short_break_minutes = 9999
long_break_minutes = 9999
sessions_before_long_break = 99
[goals]
daily_minutes = 99999
day_start_hour = 23
[music]
volume = 150
search_results = 500
)");
  CHECK(high.timer.focus == 180min);
  CHECK(high.timer.short_break == 60min);
  CHECK(high.timer.long_break == 120min);
  CHECK(high.timer.sessions_before_long_break == 12);
  CHECK(high.daily_goal_minutes == 720);
  CHECK(high.day_start_hour == 12);
  CHECK(high.music_volume == 100);
  CHECK(high.music_search_results == 20);
}

// Values of the wrong type or with a bad format fall back to the default
// for that one setting, without an error.
static void ignores_wrong_types() {
  std::string error;
  const Config config = load_text(R"(
[timer]
focus_minutes = "50"
auto_start_breaks = "yes"
[notifications]
desktop = "off"
[theme]
mode = "neon"
focus = "red"
short_break = "#12345"
long_break = 42
[music]
volume = "loud"
mpv_args = ["--no-video"]
)",
                                  &error);
  CHECK(error.empty());
  CHECK(config.timer.focus == 25min);
  CHECK(!config.timer.auto_start_breaks);
  CHECK(config.desktop_notifications);
  CHECK(config.theme == ThemeMode::Terminal);
  CHECK(config.focus_color == "#e06c75");
  CHECK(config.short_break_color == "#98c379");
  CHECK(config.long_break_color == "#61afef");
  CHECK(config.music_volume == 70);
  CHECK(config.mpv_args.empty());

  // A section that isn't a table is ignored too.
  const Config flat = load_text("timer = 5\nmusic = \"on\"\n", &error);
  CHECK(error.empty());
  CHECK(flat.timer.focus == 25min);
}

// Broken TOML gives an error and the defaults, and the user's file is left
// alone so they can fix it.
static void reports_broken_toml() {
  const std::string broken = "[timer\nfocus_minutes = 50\n";
  std::string error;
  const Config config = load_text(broken, &error);
  CHECK(!error.empty());
  CHECK(error.rfind("config.toml: ", 0) == 0);
  CHECK(config.timer.focus == 25min);
  CHECK(read_file(kDir / "config.toml") == broken);
}

int main() {
  parses_hex_colors();
  first_run_writes_defaults();
  saves_and_loads_every_setting();
  missing_keys_keep_defaults();
  clamps_values_out_of_range();
  ignores_wrong_types();
  reports_broken_toml();
  std::filesystem::remove_all(kDir);

  if (failures == 0) std::printf("all config tests passed\n");
  return failures == 0 ? 0 : 1;
}

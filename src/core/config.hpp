#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "core/timer.hpp"

namespace potter {

struct Rgb {
  std::uint8_t r = 0, g = 0, b = 0;
};

// Parses "#rrggbb" (the leading '#' is optional).
std::optional<Rgb> parse_hex_color(std::string_view text);

enum class ThemeMode { Terminal, Custom };

struct Config {
  TimerSettings timer;

  bool desktop_notifications = true;
  bool terminal_bell = true;

  // Terminal: use the terminal's own palette (follows pywal/matugen themes).
  // Custom: use the hex colors below.
  ThemeMode theme = ThemeMode::Terminal;
  std::string focus_color = "#e06c75";
  std::string short_break_color = "#98c379";
  std::string long_break_color = "#61afef";

  // Whether the task panel is shown next to the timer (toggled with 't').
  bool show_tasks = true;

  // Daily focus goal, in minutes (default: 4 × 25 min).
  int daily_goal_minutes = 100;
  // Hour (0-12) at which a new day starts, so late-night focus still counts
  // for the day before.
  int day_start_hour = 4;

  // Music (needs mpv; YouTube search also needs yt-dlp).
  int music_volume = 70;              // 0-100, remembered between runs
  bool pause_music_on_breaks = true;  // pause during breaks, resume on focus
  int music_search_results = 5;       // 1-20
  std::string mpv_args;               // extra mpv options, e.g. "--audio-device=..."
};

// $XDG_CONFIG_HOME/potter/config.toml, or ~/.config/potter/config.toml.
std::filesystem::path config_path();

// Loads the config, falling back to defaults for anything missing or
// invalid. Writes a default file on first run. On a parse error, `error`
// gets a message and defaults are returned.
Config load_config(const std::filesystem::path& path, std::string* error = nullptr);

// Returns false and fills `error` if the file couldn't be written.
bool save_config(const Config& config, const std::filesystem::path& path,
                 std::string* error = nullptr);

}  // namespace potter

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "timer.hpp"

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

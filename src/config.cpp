#include "config.hpp"

#include <algorithm>
#include <sstream>
#include <system_error>

#include <toml++/toml.hpp>

#include "storage.hpp"

namespace potter {

namespace {

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::chrono::minutes read_minutes(const toml::table& table, std::string_view key,
                                  std::chrono::minutes fallback, int max) {
  const auto value = table[key].value<int64_t>();
  if (!value) return fallback;
  return std::chrono::minutes(std::clamp<int64_t>(*value, 1, max));
}

std::string read_color(const toml::table& table, std::string_view key,
                       const std::string& fallback) {
  const auto value = table[key].value<std::string>();
  if (!value || !parse_hex_color(*value)) return fallback;
  return *value;
}

}  // namespace

std::optional<Rgb> parse_hex_color(std::string_view text) {
  if (!text.empty() && text.front() == '#') text.remove_prefix(1);
  if (text.size() != 6) return std::nullopt;
  int v[6];
  for (int i = 0; i < 6; ++i) {
    v[i] = hex_digit(text[i]);
    if (v[i] < 0) return std::nullopt;
  }
  return Rgb{static_cast<std::uint8_t>(v[0] * 16 + v[1]),
             static_cast<std::uint8_t>(v[2] * 16 + v[3]),
             static_cast<std::uint8_t>(v[4] * 16 + v[5])};
}

std::filesystem::path config_path() {
  return xdg_dir("XDG_CONFIG_HOME", ".config") / "potter" / "config.toml";
}

Config load_config(const std::filesystem::path& path, std::string* error) {
  Config config;
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    save_config(config, path);
    return config;
  }

  toml::table root;
  try {
    root = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    if (error) *error = std::string("config.toml: ") + std::string(e.description());
    return config;
  }

  if (const auto* t = root["timer"].as_table()) {
    auto& timer = config.timer;
    timer.focus = read_minutes(*t, "focus_minutes", timer.focus, 180);
    timer.short_break = read_minutes(*t, "short_break_minutes", timer.short_break, 60);
    timer.long_break = read_minutes(*t, "long_break_minutes", timer.long_break, 120);
    if (auto n = (*t)["sessions_before_long_break"].value<int64_t>()) {
      timer.sessions_before_long_break = static_cast<int>(std::clamp<int64_t>(*n, 1, 12));
    }
    timer.auto_start_breaks =
        (*t)["auto_start_breaks"].value_or(timer.auto_start_breaks);
    timer.auto_start_focus = (*t)["auto_start_focus"].value_or(timer.auto_start_focus);
  }

  if (const auto* t = root["notifications"].as_table()) {
    config.desktop_notifications = (*t)["desktop"].value_or(config.desktop_notifications);
    config.terminal_bell = (*t)["bell"].value_or(config.terminal_bell);
  }

  if (const auto* t = root["theme"].as_table()) {
    config.theme = (*t)["mode"].value_or(std::string("terminal")) == "custom"
                       ? ThemeMode::Custom
                       : ThemeMode::Terminal;
    config.focus_color = read_color(*t, "focus", config.focus_color);
    config.short_break_color = read_color(*t, "short_break", config.short_break_color);
    config.long_break_color = read_color(*t, "long_break", config.long_break_color);
  }

  if (const auto* t = root["tasks"].as_table()) {
    config.show_tasks = (*t)["show_panel"].value_or(config.show_tasks);
  }

  return config;
}

bool save_config(const Config& config, const std::filesystem::path& path,
                 std::string* error) {
  const auto& timer = config.timer;
  toml::table root{
      {"timer",
       toml::table{
           {"focus_minutes", timer.focus.count()},
           {"short_break_minutes", timer.short_break.count()},
           {"long_break_minutes", timer.long_break.count()},
           {"sessions_before_long_break", timer.sessions_before_long_break},
           {"auto_start_breaks", timer.auto_start_breaks},
           {"auto_start_focus", timer.auto_start_focus},
       }},
      {"notifications",
       toml::table{
           {"desktop", config.desktop_notifications},
           {"bell", config.terminal_bell},
       }},
      {"theme",
       toml::table{
           {"mode", config.theme == ThemeMode::Custom ? "custom" : "terminal"},
           {"focus", config.focus_color},
           {"short_break", config.short_break_color},
           {"long_break", config.long_break_color},
       }},
      {"tasks",
       toml::table{
           {"show_panel", config.show_tasks},
       }},
  };

  std::ostringstream out;
  out << "# potter config — edit here or press 's' in potter\n\n" << root << '\n';
  return write_atomically(path, out.str(), error);
}

}  // namespace potter

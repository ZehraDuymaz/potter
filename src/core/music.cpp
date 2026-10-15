#include "core/music.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include "core/storage.hpp"

namespace potter {

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

std::vector<std::string_view> split(std::string_view s, char sep) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  while (true) {
    const std::size_t end = s.find(sep, start);
    parts.push_back(s.substr(start, end - start));
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return parts;
}

}  // namespace

InputKind classify_input(std::string_view text) {
  text = trim(text);
  if (text.find("://") != std::string_view::npos) return InputKind::Url;
  if (text.starts_with("/") || text.starts_with("~/") || text == "~" ||
      text.starts_with("./") || text.starts_with("../")) {
    return InputKind::File;
  }
  return InputKind::Search;
}

std::string expand_home(std::string path) {
  if (path == "~" || path.starts_with("~/")) {
    if (const char* home = std::getenv("HOME")) path.replace(0, 1, home);
  }
  return path;
}

std::string search_print_template() {
  const std::string sep(1, kSearchFieldSep);
  return "%(id)s" + sep + "%(title)s" + sep + "%(channel)s" + sep + "%(duration)s";
}

std::optional<SearchResult> parse_search_line(std::string_view line) {
  const auto fields = split(line, kSearchFieldSep);
  if (fields.size() < 4 || trim(fields[0]).empty() || trim(fields[1]).empty()) {
    return std::nullopt;
  }
  SearchResult result;
  result.id = trim(fields[0]);
  result.title = trim(fields[1]);
  result.channel = trim(fields[2]) == "NA" ? "" : std::string(trim(fields[2]));
  const std::string duration(trim(fields[3]));
  char* end = nullptr;
  const double seconds = std::strtod(duration.c_str(), &end);
  if (!duration.empty() && end && *end == '\0' && seconds > 0) {
    result.duration_seconds = static_cast<int>(std::lround(seconds));
  }
  return result;
}

std::string format_clock(double seconds) {
  const long long total = std::max(0LL, static_cast<long long>(seconds));
  char buf[32];
  if (total >= 3600) {
    std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", total / 3600, (total / 60) % 60,
                  total % 60);
  } else {
    std::snprintf(buf, sizeof(buf), "%lld:%02lld", total / 60, total % 60);
  }
  return buf;
}

void apply_mpv_message(PlayerState& state, std::string_view line) {
  const auto msg = nlohmann::json::parse(line, nullptr, /*allow_exceptions=*/false);
  if (!msg.is_object()) return;
  const std::string event = msg.value("event", "");

  if (event == "property-change") {
    const std::string name = msg.value("name", "");
    // A missing "data" means the property is currently unavailable.
    const auto it = msg.find("data");
    const bool has = it != msg.end() && !it->is_null();
    if (name == "media-title") {
      state.title = has && it->is_string() ? it->get<std::string>() : "";
    } else if (name == "path") {
      state.path = has && it->is_string() ? it->get<std::string>() : "";
    } else if (name == "pause") {
      state.paused = has && it->is_boolean() && it->get<bool>();
    } else if (name == "idle-active") {
      state.idle = !has || !it->is_boolean() || it->get<bool>();
    } else if (name == "volume") {
      if (has && it->is_number()) state.volume = it->get<double>();
    } else if (name == "time-pos") {
      state.time_pos = has && it->is_number() ? std::optional(it->get<double>()) : std::nullopt;
    } else if (name == "duration") {
      state.duration = has && it->is_number() ? std::optional(it->get<double>()) : std::nullopt;
    }
  } else if (event == "start-file") {
    state.error.clear();
  } else if (event == "file-loaded") {
    ++state.files_loaded;
  } else if (event == "end-file" && msg.value("reason", "") == "error") {
    state.error = "couldn't play this: " + msg.value("file_error", std::string("unknown error"));
  }
}

bool Favorites::add(std::string title, std::string target) {
  if (target.empty() || contains(target)) return false;
  if (title.empty()) title = target;
  items_.push_back({std::move(title), std::move(target)});
  dirty_ = true;
  return true;
}

bool Favorites::remove(std::size_t index) {
  if (index >= items_.size()) return false;
  items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
  dirty_ = true;
  return true;
}

bool Favorites::remove_target(const std::string& target) {
  const auto it = std::find_if(items_.begin(), items_.end(),
                               [&](const Favorite& f) { return f.target == target; });
  if (it == items_.end()) return false;
  return remove(static_cast<std::size_t>(it - items_.begin()));
}

bool Favorites::contains(const std::string& target) const {
  return std::any_of(items_.begin(), items_.end(),
                     [&](const Favorite& f) { return f.target == target; });
}

bool Favorites::load(const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return true;

  toml::table root;
  try {
    root = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    if (error) *error = "favorites.toml: " + std::string(e.description());
    return false;
  }

  std::vector<Favorite> loaded;
  if (const toml::array* list = root["favorite"].as_array()) {
    for (const toml::node& node : *list) {
      const toml::table* t = node.as_table();
      if (!t) continue;
      Favorite f;
      f.target = (*t)["target"].value_or(std::string());
      f.title = (*t)["title"].value_or(f.target);
      if (!f.target.empty()) loaded.push_back(std::move(f));
    }
  }
  items_ = std::move(loaded);
  dirty_ = false;
  return true;
}

bool Favorites::save(const std::filesystem::path& path, std::string* error) {
  toml::array list;
  for (const Favorite& f : items_) {
    list.push_back(toml::table{{"title", f.title}, {"target", f.target}});
  }
  toml::table root{{"favorite", std::move(list)}};

  std::ostringstream out;
  out << "# potter favorites — press f in potter's music screen to add\n\n" << root << '\n';
  if (!write_atomically(path, out.str(), error)) return false;
  dirty_ = false;
  return true;
}

std::filesystem::path favorites_path() {
  return xdg_dir("XDG_DATA_HOME", ".local/share") / "potter" / "favorites.toml";
}

std::filesystem::path mpv_config_dir() {
  return xdg_dir("XDG_CONFIG_HOME", ".config") / "potter" / "mpv";
}

}  // namespace potter

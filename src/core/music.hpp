#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace potter {

// What the text typed into the music box means, like `play` in a shell:
// a link plays directly, a path plays a local file or folder, anything else
// searches YouTube.
enum class InputKind { Search, Url, File };
InputKind classify_input(std::string_view text);

// "~/Music" -> "/home/me/Music".
std::string expand_home(std::string path);

// One YouTube search result, as printed by yt-dlp.
struct SearchResult {
  std::string id;
  std::string title;
  std::string channel;
  std::optional<int> duration_seconds;  // missing for live streams

  std::string url() const { return "https://youtu.be/" + id; }
};

// Separator between fields in yt-dlp's --print output (ASCII "unit
// separator", which never appears in titles).
inline constexpr char kSearchFieldSep = '\x1f';
// The --print template that parse_search_line() understands.
std::string search_print_template();
std::optional<SearchResult> parse_search_line(std::string_view line);

// 65 -> "1:05", 3723 -> "1:02:03".
std::string format_clock(double seconds);

// What mpv is doing, built from the messages on its control socket.
struct PlayerState {
  bool connected = false;  // mpv is running and potter is talking to it
  bool idle = true;        // nothing loaded
  bool paused = false;
  double volume = 0;
  std::optional<double> time_pos;
  std::optional<double> duration;
  std::string title;  // mpv's media-title
  std::string path;   // what was loaded (URL or file)
  std::string error;  // why the last file failed to play, if it did
  int files_loaded = 0;  // counts mpv's file-loaded events

  bool playing() const { return connected && !idle && !paused; }
  bool has_media() const { return connected && !idle; }
};

// Applies one JSON line from mpv's IPC socket to `state`. Lines that aren't
// events potter cares about are ignored.
void apply_mpv_message(PlayerState& state, std::string_view line);

struct Favorite {
  std::string title;
  std::string target;  // URL or file path
};

// Saved tracks and streams. Pure logic, no UI.
class Favorites {
 public:
  // Returns false if the target is already a favorite.
  bool add(std::string title, std::string target);
  bool remove(std::size_t index);
  bool remove_target(const std::string& target);
  bool contains(const std::string& target) const;
  const std::vector<Favorite>& list() const { return items_; }

  bool dirty() const { return dirty_; }
  // A missing file is an empty list, not an error.
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  bool save(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  std::vector<Favorite> items_;
  bool dirty_ = false;
};

// $XDG_DATA_HOME/potter/favorites.toml, or ~/.local/share/potter/favorites.toml.
std::filesystem::path favorites_path();

// The same track under one name: YouTube links by video id (so youtu.be/x and
// youtube.com/watch?v=x match), anything else as given.
std::string track_key(std::string_view target);

// Where you left off in each track, so it can be resumed later. Pure logic,
// no UI. Newest first, at most kMaxEntries.
class ResumePoints {
 public:
  static constexpr std::size_t kMaxEntries = 200;
  // The start and end of a track don't count as "left off".
  static constexpr double kMargin = 15;

  // Notes the position of a playing track. Positions near the start or end
  // forget the track; live streams (no duration) and short tracks are ignored.
  void record(const std::string& target, double position, std::optional<double> duration);
  // Whole seconds into the track, if there's a position worth resuming.
  std::optional<int> get(const std::string& target) const;

  bool dirty() const { return dirty_; }
  // A missing file is an empty list, not an error.
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  bool save(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  struct Entry {
    std::string key;
    int seconds;
  };
  std::vector<Entry> entries_;
  bool dirty_ = false;
};

// $XDG_DATA_HOME/potter/positions.toml, or ~/.local/share/potter/positions.toml.
std::filesystem::path resume_points_path();

// The config folder potter's mpv uses instead of ~/.config/mpv:
// $XDG_CONFIG_HOME/potter/mpv, or ~/.config/potter/mpv.
std::filesystem::path mpv_config_dir();

}  // namespace potter

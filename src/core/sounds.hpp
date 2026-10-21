#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace potter {

// A background sound for the sounds screen: a recording that loops, or noise
// that mpv generates.
struct Sound {
  std::string id;     // "rain"; for your own files, the file name
  std::string name;   // "Rain"
  std::string file;   // file name in the sounds folder; empty for generated noise
  std::string noise;  // mpv/ffmpeg noise source for generated sounds
  int size_kb = 0;    // download size of a built-in recording
  bool yours = false; // a file you added to the sounds folder

  bool generated() const { return file.empty(); }
};

// The sounds potter knows, in the order they're listed. Recordings come from
// Blanket (see SOUNDS.md) and are downloaded on first use.
const std::vector<Sound>& builtin_sounds();

// The built-in sounds plus any audio files you put in `dir`, sorted by name
// after the built-ins.
std::vector<Sound> list_sounds(const std::filesystem::path& dir);

// Where a built-in recording is downloaded from.
std::string sound_url(const Sound& sound);

// What mpv plays for a sound: the file in `dir`, or the noise source.
std::string sound_target(const Sound& sound, const std::filesystem::path& dir);

// "~3.0 MB" for a download size.
std::string format_size(int size_kb);

// Remembered between runs: each sound's volume and the last mix that played.
// Pure logic, no UI.
class SoundMix {
 public:
  static constexpr int kDefaultVolume = 50;

  int volume(const std::string& id) const;
  void set_volume(const std::string& id, int volume);
  const std::vector<std::string>& last_mix() const { return last_mix_; }
  // Called whenever the playing sounds change; an empty mix keeps the old
  // one, so turning everything off can be undone.
  void set_last_mix(std::vector<std::string> ids);

  bool dirty() const { return dirty_; }
  // A missing file is an empty mix, not an error.
  bool load(const std::filesystem::path& path, std::string* error = nullptr);
  bool save(const std::filesystem::path& path, std::string* error = nullptr);

 private:
  std::map<std::string, int> volumes_;
  std::vector<std::string> last_mix_;
  bool dirty_ = false;
};

// $XDG_DATA_HOME/potter/sounds, or ~/.local/share/potter/sounds.
std::filesystem::path sounds_dir();
// $XDG_DATA_HOME/potter/sounds.toml, or ~/.local/share/potter/sounds.toml.
std::filesystem::path sound_mix_path();

}  // namespace potter

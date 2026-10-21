#include "core/sounds.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <system_error>

#include <toml++/toml.hpp>

#include "core/storage.hpp"

namespace potter {

namespace {

// The recordings live in potter's repository. Files there are never renamed
// or removed, so older potter versions keep working.
constexpr const char* kSoundsUrl = "https://raw.githubusercontent.com/ZehraDuymaz/potter/main/sounds/";

bool is_audio_file(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  for (const char* known : {".ogg", ".opus", ".mp3", ".flac", ".wav", ".m4a"}) {
    if (ext == known) return true;
  }
  return false;
}

}  // namespace

const std::vector<Sound>& builtin_sounds() {
  // Noise levels roughly match the recordings' loudness; white noise is a
  // little quieter since it sounds harsh.
  static const std::vector<Sound> sounds = {
      {"rain", "Rain", "rain.ogg", "", 3061},
      {"storm", "Storm", "storm.ogg", "", 475},
      {"wind", "Wind", "wind.ogg", "", 283},
      {"waves", "Waves", "waves.ogg", "", 2701},
      {"stream", "Stream", "stream.ogg", "", 4908},
      {"birds", "Birds", "birds.ogg", "", 2992},
      {"summer-night", "Summer night", "summer-night.ogg", "", 599},
      {"fireplace", "Fireplace", "fireplace.ogg", "", 318},
      {"coffee-shop", "Coffee shop", "coffee-shop.ogg", "", 260},
      {"city", "City", "city.ogg", "", 352},
      {"train", "Train", "train.ogg", "", 124},
      {"boat", "Boat", "boat.ogg", "", 621},
      {"white-noise", "White noise", "", "anoisesrc=color=white:amplitude=0.035"},
      {"pink-noise", "Pink noise", "", "anoisesrc=color=pink:amplitude=0.1"},
      {"brown-noise", "Brown noise", "", "anoisesrc=color=brown:amplitude=0.15"},
  };
  return sounds;
}

std::vector<Sound> list_sounds(const std::filesystem::path& dir) {
  std::vector<Sound> list = builtin_sounds();

  std::vector<Sound> yours;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    const auto& path = entry.path();
    // Half-finished downloads end in ".part", so they're skipped here too.
    if (!entry.is_regular_file(ec) || !is_audio_file(path)) continue;
    const std::string file = path.filename().string();
    const bool builtin = std::any_of(list.begin(), list.end(),
                                     [&](const Sound& s) { return s.file == file; });
    if (builtin) continue;
    Sound s;
    s.id = file;
    s.name = path.stem().string();
    s.file = file;
    s.yours = true;
    yours.push_back(std::move(s));
  }
  // Alphabetical, ignoring case.
  auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
  };
  std::sort(yours.begin(), yours.end(),
            [&](const Sound& a, const Sound& b) { return lower(a.name) < lower(b.name); });
  list.insert(list.end(), yours.begin(), yours.end());
  return list;
}

std::string sound_url(const Sound& sound) { return kSoundsUrl + sound.file; }

std::string sound_target(const Sound& sound, const std::filesystem::path& dir) {
  if (sound.generated()) return "av://lavfi:" + sound.noise;
  return (dir / sound.file).string();
}

std::string format_size(int size_kb) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f MB", size_kb / 1024.0);
  return buf;
}

int SoundMix::volume(const std::string& id) const {
  const auto it = volumes_.find(id);
  return it == volumes_.end() ? kDefaultVolume : it->second;
}

void SoundMix::set_volume(const std::string& id, int volume) {
  volume = std::clamp(volume, 0, 100);
  if (this->volume(id) == volume) return;
  volumes_[id] = volume;
  dirty_ = true;
}

void SoundMix::set_last_mix(std::vector<std::string> ids) {
  if (ids.empty() || ids == last_mix_) return;
  last_mix_ = std::move(ids);
  dirty_ = true;
}

bool SoundMix::load(const std::filesystem::path& path, std::string* error) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return true;

  toml::table root;
  try {
    root = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    if (error) *error = "sounds.toml: " + std::string(e.description());
    return false;
  }

  std::map<std::string, int> volumes;
  if (const toml::table* t = root["volume"].as_table()) {
    for (const auto& [key, node] : *t) {
      if (const auto v = node.value<int64_t>()) {
        volumes[std::string(key.str())] = static_cast<int>(std::clamp<int64_t>(*v, 0, 100));
      }
    }
  }
  std::vector<std::string> mix;
  if (const toml::array* list = root["last_mix"].as_array()) {
    for (const toml::node& node : *list) {
      if (const auto id = node.value<std::string>(); id && !id->empty()) mix.push_back(*id);
    }
  }
  volumes_ = std::move(volumes);
  last_mix_ = std::move(mix);
  dirty_ = false;
  return true;
}

bool SoundMix::save(const std::filesystem::path& path, std::string* error) {
  toml::table volumes;
  for (const auto& [id, volume] : volumes_) volumes.insert(id, volume);
  toml::array mix;
  for (const std::string& id : last_mix_) mix.push_back(id);
  toml::table root{{"last_mix", std::move(mix)}, {"volume", std::move(volumes)}};

  std::ostringstream out;
  out << "# potter sounds: the last mix (r brings it back) and each sound's volume\n\n"
      << root << '\n';
  if (!write_atomically(path, out.str(), error)) return false;
  dirty_ = false;
  return true;
}

std::filesystem::path sounds_dir() {
  return xdg_dir("XDG_DATA_HOME", ".local/share") / "potter" / "sounds";
}

std::filesystem::path sound_mix_path() {
  return xdg_dir("XDG_DATA_HOME", ".local/share") / "potter" / "sounds.toml";
}

}  // namespace potter

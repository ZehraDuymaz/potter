// Unit tests for the pure sounds logic: the sound list and the remembered
// mix. Same style as timer_test.cpp.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "core/sounds.hpp"

using namespace potter;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static void has_builtin_sounds() {
  const auto& sounds = builtin_sounds();
  std::set<std::string> ids;
  for (const Sound& s : sounds) {
    CHECK(ids.insert(s.id).second);  // ids are unique
    CHECK(!s.name.empty());
    CHECK(s.generated() == s.file.empty());
    CHECK(s.generated() ? !s.noise.empty() : s.size_kb > 0);
  }
  CHECK(sounds.front().id == "rain");

  const Sound& rain = sounds.front();
  CHECK(sound_url(rain) ==
        "https://raw.githubusercontent.com/ZehraDuymaz/potter/main/sounds/rain.ogg");
  CHECK(sound_target(rain, "/data/sounds") == "/data/sounds/rain.ogg");
  const Sound& brown = sounds.back();
  CHECK(brown.id == "brown-noise");
  CHECK(sound_target(brown, "/data/sounds") == "av://lavfi:" + brown.noise);
  CHECK(format_size(3061) == "3.0 MB");
}

static void lists_your_own_files() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-sounds-test";
  std::filesystem::remove_all(dir);
  CHECK(list_sounds(dir).size() == builtin_sounds().size());  // missing folder is fine

  std::filesystem::create_directories(dir);
  for (const char* name : {"rain.ogg", "Zen bells.mp3", "airport.OGG", "notes.txt",
                           "stream.ogg.part"}) {
    std::ofstream(dir / name) << "x";
  }
  const auto list = list_sounds(dir);
  // Downloaded built-ins aren't listed twice; text files and half-finished
  // downloads aren't sounds.
  CHECK(list.size() == builtin_sounds().size() + 2);
  const Sound& first_yours = list[builtin_sounds().size()];
  CHECK(first_yours.yours);
  CHECK(first_yours.name == "airport");  // alphabetical, ignoring case
  CHECK(sound_target(first_yours, dir) == (dir / "airport.OGG").string());
  CHECK(list.back().name == "Zen bells");
  CHECK(list.back().id == "Zen bells.mp3");
  std::filesystem::remove_all(dir);
}

static void remembers_the_mix() {
  const auto dir = std::filesystem::temp_directory_path() / "potter-sounds-mix-test";
  std::filesystem::remove_all(dir);
  const auto path = dir / "sounds.toml";

  SoundMix mix;
  CHECK(mix.load(path));  // missing file = defaults
  CHECK(mix.volume("rain") == SoundMix::kDefaultVolume);
  CHECK(!mix.dirty());
  mix.set_volume("rain", SoundMix::kDefaultVolume);  // no change
  CHECK(!mix.dirty());
  mix.set_volume("rain", 130);  // clamped
  CHECK(mix.volume("rain") == 100);
  mix.set_volume("brown-noise", -5);
  CHECK(mix.volume("brown-noise") == 0);

  mix.set_last_mix({"rain", "brown-noise"});
  mix.set_last_mix({});  // everything off keeps the last mix
  CHECK(mix.last_mix().size() == 2);
  CHECK(mix.dirty());
  CHECK(mix.save(path));
  CHECK(!mix.dirty());

  SoundMix loaded;
  CHECK(loaded.load(path));
  CHECK(loaded.volume("rain") == 100);
  CHECK(loaded.volume("brown-noise") == 0);
  CHECK(loaded.volume("storm") == SoundMix::kDefaultVolume);
  CHECK(loaded.last_mix() == std::vector<std::string>({"rain", "brown-noise"}));

  std::ofstream(path) << "last_mix = [";
  std::string error;
  CHECK(!loaded.load(path, &error));
  CHECK(!error.empty());
  std::filesystem::remove_all(dir);
}

int main() {
  has_builtin_sounds();
  lists_your_own_files();
  remembers_the_mix();

  if (failures == 0) std::printf("all sounds tests passed\n");
  return failures == 0 ? 0 : 1;
}

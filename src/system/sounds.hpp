#pragma once

#include <sys/types.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/sounds.hpp"
#include "system/player.hpp"

namespace potter {

// Plays background sounds: one looping mpv per sound, so each has its own
// volume. Built-in recordings are downloaded with curl the first time they're
// turned on. Everything stops when the board is destroyed.
class SoundBoard {
 public:
  SoundBoard(std::filesystem::path dir, std::string mpv_args);
  ~SoundBoard();
  SoundBoard(const SoundBoard&) = delete;
  SoundBoard& operator=(const SoundBoard&) = delete;

  // Turns a sound on at `volume` (0-100). A recording that isn't downloaded
  // yet is downloaded first and starts once it's there.
  void turn_on(const Sound& sound, int volume);
  void turn_off(const std::string& id);
  void all_off();
  void set_volume(const std::string& id, int volume);
  // Pauses or resumes every sound (for breaks).
  void set_pause(bool paused);
  // Pauses the sounds that are on, or resumes them. Returns false if none
  // are on.
  bool toggle_pause();
  bool paused() const { return paused_; }

  // On: playing, or downloading to play.
  bool on(const std::string& id) const;
  bool downloading(const std::string& id) const;
  // The sounds that are on, in the order they were turned on.
  const std::vector<std::string>& active() const { return active_; }
  // What went wrong since the last call (a failed download, ...), or "".
  std::string take_error();

  // Starts sounds whose download finished. Call regularly from the UI thread.
  void update();

 private:
  struct Download {
    std::atomic<bool> done = false;
    std::atomic<bool> ok = false;
    std::atomic<pid_t> pid = -1;  // curl, so it can be stopped on quit
    // Last, so it's destroyed (joined) before the fields it writes to.
    std::jthread thread;
  };
  struct Wanted {
    Sound sound;
    int volume;
  };

  void start_player(const Sound& sound, int volume);
  void start_download(const Sound& sound);

  std::filesystem::path dir_;
  std::string mpv_args_;
  int next_player_ = 0;  // numbers the players' control sockets
  bool paused_ = false;
  std::map<std::string, std::unique_ptr<MpvPlayer>> players_;
  std::map<std::string, std::unique_ptr<Download>> downloads_;
  // Turned on but not playing yet: still downloading, or downloaded during a
  // break (they start when the break ends).
  std::map<std::string, Wanted> waiting_;
  std::vector<std::string> active_;
  std::string error_;
};

}  // namespace potter

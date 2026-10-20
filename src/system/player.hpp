#pragma once

#include <sys/types.h>

#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "core/music.hpp"

namespace potter {

// Plays music through mpv running in the background, controlled over mpv's
// JSON IPC socket. mpv uses yt-dlp itself for YouTube links. mpv is started
// on first use and stopped when the player is destroyed, so music never
// outlives potter.
class MpvPlayer {
 public:
  MpvPlayer(int volume, std::string extra_args);
  ~MpvPlayer();
  MpvPlayer(const MpvPlayer&) = delete;
  MpvPlayer& operator=(const MpvPlayer&) = delete;

  // Whether mpv is installed.
  static bool available();

  // Plays a URL or file, replacing what's playing. `title` is shown until
  // mpv reports the real title. Returns false if mpv couldn't be started.
  bool play(const std::string& target, const std::string& title = "");
  void toggle_pause();
  void set_pause(bool paused);
  // Jumps in what's playing. While a file is still loading, the newest jump
  // waits until it has loaded.
  // Moves `seconds` forward (or back, if negative).
  void seek_by(double seconds);
  // Jumps to `percent` (0-100) of the track.
  void seek_percent(int percent);
  // Jumps to `seconds` from the start.
  void seek_to(double seconds);
  // Changes the volume by `delta` (clamped to 0-100); returns the new volume.
  int change_volume(int delta);
  // Stops playback and quits mpv.
  void stop();

  // A snapshot of the current state, safe to use from the UI thread.
  PlayerState state() const;
  // The best title to show for what's playing.
  std::string display_title() const;
  // True between play() and the moment mpv actually starts the file (for
  // YouTube links this includes yt-dlp's lookup, which takes a few seconds).
  bool loading() const;
  int volume() const { return volume_; }

 private:
  bool ensure_started();
  static std::string command_line(const std::vector<std::string>& command);
  static std::string seek_command(double amount, const char* mode);
  void send(const std::vector<std::string>& command);
  void seek(double amount, const char* mode);
  void io_loop(std::stop_token stop);

  int volume_;
  std::string extra_args_;
  std::string socket_path_;
  pid_t pid_ = -1;
  int wake_[2] = {-1, -1};  // pipe to wake the IO thread when commands are queued

  mutable std::mutex mutex_;
  PlayerState state_;
  std::string title_hint_;
  bool requested_ = false;  // play() called, file not started yet
  int loaded_at_request_ = 0;
  struct PendingSeek {
    double amount;
    const char* mode;  // an mpv seek mode, e.g. "relative"
  };
  std::optional<PendingSeek> pending_seek_;  // a jump asked for while loading
  std::vector<std::string> outbox_;  // JSON lines waiting to be sent
  std::jthread io_;
};

}  // namespace potter

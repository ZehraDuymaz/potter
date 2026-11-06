#pragma once

#include <sys/types.h>

#include <map>
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
  // `name` keeps the control sockets of several players apart.
  MpvPlayer(int volume, std::string extra_args, const std::string& name = "music");
  ~MpvPlayer();
  MpvPlayer(const MpvPlayer&) = delete;
  MpvPlayer& operator=(const MpvPlayer&) = delete;

  // Whether mpv is installed.
  static bool available();

  // Plays a URL or file now, in place of the current track; the tracks up
  // next stay queued. `title` is shown until mpv reports the real title.
  // YouTube playlist links expand into the queue. Returns false if mpv
  // couldn't be started.
  bool play(const std::string& target, const std::string& title = "");
  // Adds a track to the end of the queue, or plays it if nothing is playing.
  bool enqueue(const std::string& target, const std::string& title = "");
  // Plays the queue entry at `index`.
  void play_index(int index);
  // Removes the queue entry at `index` (the current one too: then the next plays).
  void remove_index(int index);
  // Removes every queue entry except the current one.
  void clear_queue();
  void next();
  void previous();
  // Off -> track -> queue -> off. Kept when mpv restarts.
  LoopMode cycle_loop();
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
  void set_volume(int volume);
  // Stops playback and quits mpv.
  void stop();

  // A snapshot of the current state, safe to use from the UI thread.
  PlayerState state() const;
  // The best title to show for what's playing.
  std::string display_title() const;
  // The best title to show for a queue entry.
  std::string entry_title(const QueueEntry& entry) const;
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
  // Marks a new track as asked for, so loading() is true until it starts.
  void begin_request(const std::string& target, const std::string& title);
  void io_loop(std::stop_token stop);

  int volume_;
  LoopMode loop_ = LoopMode::Off;
  std::string extra_args_;
  std::string socket_path_;
  std::string script_path_;  // kWaitScript, written when mpv starts
  pid_t pid_ = -1;
  int wake_[2] = {-1, -1};  // pipe to wake the IO thread when commands are queued

  mutable std::mutex mutex_;
  PlayerState state_;
  std::string title_hint_;
  // Titles potter knew when it queued a target (e.g. from search results).
  std::map<std::string, std::string> known_titles_;
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

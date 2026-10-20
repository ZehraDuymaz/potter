#include "system/player.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sstream>

#include <nlohmann/json.hpp>

#include "system/process.hpp"

namespace potter {

namespace {

// Properties potter watches, in the order they're observed.
constexpr const char* kObserved[] = {"media-title", "path",     "pause",   "idle-active",
                                     "volume",      "time-pos", "duration"};

std::filesystem::path runtime_dir() {
  for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
    if (const char* dir = std::getenv(var); dir && *dir) return dir;
  }
  return "/tmp";
}

int connect_unix(const std::string& path) {
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

bool write_all(int fd, const std::string& data) {
  std::size_t done = 0;
  while (done < data.size()) {
    const ssize_t n = write(fd, data.data() + done, data.size() - done);
    if (n <= 0) return false;
    done += static_cast<std::size_t>(n);
  }
  return true;
}

}  // namespace

MpvPlayer::MpvPlayer(int volume, std::string extra_args)
    : volume_(std::clamp(volume, 0, 100)), extra_args_(std::move(extra_args)) {
  socket_path_ =
      (runtime_dir() / ("potter-mpv-" + std::to_string(getpid()) + ".sock")).string();
}

MpvPlayer::~MpvPlayer() { stop(); }

bool MpvPlayer::available() { return in_path("mpv"); }

bool MpvPlayer::ensure_started() {
  if (pid_ > 0) {
    if (waitpid(pid_, nullptr, WNOHANG) == 0) return true;  // still running
    pid_ = -1;  // it exited on its own (and is now reaped)
  }
  stop();  // clean up the old connection, if any

  std::filesystem::remove(socket_path_);
  std::vector<std::string> args = {
      "mpv",
      "--idle=yes",
      // potter's own config folder instead of ~/.config/mpv, so settings meant
      // for watching videos (`no-audio` for a wallpaper, `loop-file`, ...)
      // don't break music.
      "--config-dir=" + mpv_config_dir().string(),
      "--no-video",
      // Command-line options beat mpv.conf, so audio stays on even if that
      // folder's mpv.conf includes one with `no-audio`.
      "--aid=auto",
      "--no-terminal",
      "--force-window=no",
      "--ytdl-format=bestaudio/best",
      // YouTube sends one long download barely faster than it plays, so
      // jumping ahead meant waiting for a new download. Asking in 1 MiB
      // pieces gets full speed, so mpv buffers far ahead and jumps are
      // instant. (Ignored by ffmpeg older than 7.1.)
      "--stream-lavf-o=request_size=1048576",
      // With fast downloads mpv would fill its whole 150 MiB buffer. A
      // small one is enough now (about 8 minutes of audio ahead, 4 behind),
      // since a fresh download is quick, and keeps the RAM low.
      "--demuxer-max-bytes=8MiB",
      "--demuxer-max-back-bytes=4MiB",
      "--input-ipc-server=" + socket_path_,
      "--volume=" + std::to_string(volume_),
  };
  std::istringstream extra(extra_args_);
  for (std::string arg; extra >> arg;) args.push_back(arg);

  pid_ = spawn(args);
  if (pid_ < 0) return false;

  if (pipe(wake_) != 0) return false;
  for (int fd : wake_) fcntl(fd, F_SETFD, FD_CLOEXEC);
  fcntl(wake_[0], F_SETFL, O_NONBLOCK);

  {
    std::lock_guard lock(mutex_);
    state_ = PlayerState{};
    state_.volume = volume_;
  }
  io_ = std::jthread([this](std::stop_token stop) { io_loop(stop); });
  return true;
}

void MpvPlayer::io_loop(std::stop_token stop) {
  // mpv creates its socket shortly after starting; wait up to 5 seconds.
  int fd = -1;
  for (int attempt = 0; attempt < 100 && fd < 0 && !stop.stop_requested(); ++attempt) {
    fd = connect_unix(socket_path_);
    if (fd < 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (fd < 0) {
    std::lock_guard lock(mutex_);
    state_.error = "couldn't connect to mpv";
    return;
  }

  std::string initial;
  for (int i = 0; i < static_cast<int>(std::size(kObserved)); ++i) {
    initial += nlohmann::json{{"command", {"observe_property", i + 1, kObserved[i]}}}.dump() + "\n";
  }
  bool ok = write_all(fd, initial);
  {
    std::lock_guard lock(mutex_);
    state_.connected = ok;
  }

  std::string pending;  // incoming bytes not yet ending in a newline
  char buf[8192];
  while (ok && !stop.stop_requested()) {
    pollfd fds[2] = {{fd, POLLIN, 0}, {wake_[0], POLLIN, 0}};
    if (poll(fds, 2, 200) < 0) continue;

    if (fds[1].revents & POLLIN) {
      while (read(wake_[0], buf, sizeof(buf)) > 0) {
      }
    }
    std::vector<std::string> outgoing;
    {
      std::lock_guard lock(mutex_);
      outgoing.swap(outbox_);
    }
    for (const std::string& line : outgoing) ok = ok && write_all(fd, line);

    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      const ssize_t n = read(fd, buf, sizeof(buf));
      if (n <= 0) break;  // mpv went away
      pending.append(buf, static_cast<std::size_t>(n));
      std::lock_guard lock(mutex_);
      for (std::size_t nl; (nl = pending.find('\n')) != std::string::npos;) {
        apply_mpv_message(state_, std::string_view(pending).substr(0, nl));
        pending.erase(0, nl + 1);
      }
      if (state_.files_loaded != loaded_at_request_ || !state_.error.empty()) {
        if (requested_ && pending_seek_ && state_.error.empty()) {
          outbox_.push_back(seek_command(pending_seek_->amount, pending_seek_->mode));
        }
        requested_ = false;
        pending_seek_.reset();
      }
    }
  }

  close(fd);
  std::lock_guard lock(mutex_);
  state_.connected = false;
  state_.idle = true;
}

std::string MpvPlayer::command_line(const std::vector<std::string>& command) {
  nlohmann::json args = nlohmann::json::array();
  for (const std::string& part : command) args.push_back(part);
  return nlohmann::json{{"command", args}}.dump() + "\n";
}

std::string MpvPlayer::seek_command(double amount, const char* mode) {
  return command_line({"seek", std::to_string(amount), mode});
}

void MpvPlayer::send(const std::vector<std::string>& command) {
  {
    std::lock_guard lock(mutex_);
    outbox_.push_back(command_line(command));
  }
  if (wake_[1] >= 0) {
    const char byte = 1;
    [[maybe_unused]] const ssize_t n = write(wake_[1], &byte, 1);
  }
}

bool MpvPlayer::play(const std::string& target, const std::string& title) {
  if (!ensure_started()) {
    std::lock_guard lock(mutex_);
    state_.error = "couldn't start mpv";
    return false;
  }
  {
    std::lock_guard lock(mutex_);
    title_hint_ = title;
    requested_ = true;
    loaded_at_request_ = state_.files_loaded;
    pending_seek_.reset();
    state_.error.clear();
  }
  send({"loadfile", target, "replace"});
  // mpv stays paused across files, so make sure the new one plays.
  send({"set_property", "pause", "no"});
  return true;
}

void MpvPlayer::toggle_pause() {
  if (state().has_media()) send({"cycle", "pause"});
}

void MpvPlayer::set_pause(bool paused) {
  if (state().has_media()) send({"set_property", "pause", paused ? "yes" : "no"});
}

void MpvPlayer::seek_by(double seconds) { seek(seconds, "relative"); }

void MpvPlayer::seek_percent(int percent) {
  seek(std::clamp(percent, 0, 100), "absolute-percent");
}

void MpvPlayer::seek_to(double seconds) { seek(seconds, "absolute"); }

void MpvPlayer::seek(double amount, const char* mode) {
  {
    std::lock_guard lock(mutex_);
    // Still loading: remember the newest jump and make it once loaded.
    if (requested_ && state_.error.empty()) {
      pending_seek_ = PendingSeek{amount, mode};
      return;
    }
    if (!state_.has_media()) return;
  }
  send({"seek", std::to_string(amount), mode});
}

int MpvPlayer::change_volume(int delta) {
  const PlayerState s = state();
  const double current = s.connected ? s.volume : volume_;
  volume_ = std::clamp(static_cast<int>(std::lround(current)) + delta, 0, 100);
  if (s.connected) send({"set_property", "volume", std::to_string(volume_)});
  return volume_;
}

void MpvPlayer::stop() {
  if (pid_ > 0) {
    send({"quit"});
    // Give mpv a moment to quit cleanly, then make sure it's gone.
    bool exited = false;
    for (int i = 0; i < 20 && !exited; ++i) {
      exited = waitpid(pid_, nullptr, WNOHANG) == pid_;
      if (!exited) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (!exited) {
      kill(pid_, SIGTERM);
      waitpid(pid_, nullptr, 0);
    }
    pid_ = -1;
  }
  if (io_.joinable()) {
    io_.request_stop();
    io_.join();
  }
  for (int& fd : wake_) {
    if (fd >= 0) close(fd);
    fd = -1;
  }
  std::error_code ec;
  std::filesystem::remove(socket_path_, ec);

  std::lock_guard lock(mutex_);
  state_ = PlayerState{};
  title_hint_.clear();
  requested_ = false;
  pending_seek_.reset();
  outbox_.clear();
}

bool MpvPlayer::loading() const {
  std::lock_guard lock(mutex_);
  return requested_ && state_.error.empty();
}

PlayerState MpvPlayer::state() const {
  std::lock_guard lock(mutex_);
  return state_;
}

std::string MpvPlayer::display_title() const {
  std::lock_guard lock(mutex_);
  // Until yt-dlp resolves a link, mpv's title is just a piece of the URL,
  // so show the title we already know (e.g. from the search result).
  const bool is_link = state_.path.find("://") != std::string::npos;
  const bool unresolved = is_link && state_.path.find(state_.title) != std::string::npos;
  if (!state_.title.empty() && !unresolved) return state_.title;
  if (!title_hint_.empty()) return title_hint_;
  return state_.title.empty() ? state_.path : state_.title;
}

}  // namespace potter

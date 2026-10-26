#include "system/sounds.hpp"

#include <signal.h>
#include <sys/wait.h>

#include <algorithm>
#include <system_error>

#include "system/process.hpp"

namespace potter {

SoundBoard::SoundBoard(std::filesystem::path dir, std::string mpv_args)
    : dir_(std::move(dir)), mpv_args_(std::move(mpv_args)) {}

SoundBoard::~SoundBoard() {
  for (auto& [id, download] : downloads_) {
    if (const pid_t pid = download->pid; pid > 0) kill(pid, SIGTERM);
  }
  downloads_.clear();  // joins the threads
  players_.clear();    // stops mpv
}

void SoundBoard::turn_on(const Sound& sound, int volume) {
  // Turning something on by hand ends a break's pause for every sound.
  if (paused_) set_pause(false);

  if (const auto it = players_.find(sound.id); it != players_.end()) {
    it->second->set_volume(volume);
    return;
  }
  if (waiting_.count(sound.id)) {
    waiting_[sound.id].volume = volume;
    return;
  }

  std::error_code ec;
  if (!sound.generated() && !std::filesystem::exists(dir_ / sound.file, ec)) {
    if (sound.yours) {
      error_ = "can't find " + (dir_ / sound.file).string();
      return;
    }
    if (!in_path("curl")) {
      error_ = "downloading sounds needs curl — install it, then try again";
      return;
    }
    waiting_[sound.id] = {sound, volume};
    active_.push_back(sound.id);
    if (!downloads_.count(sound.id)) start_download(sound);
    return;
  }

  start_player(sound, volume);
}

void SoundBoard::start_player(const Sound& sound, int volume) {
  auto player = std::make_unique<MpvPlayer>(volume, "--loop-file=inf " + mpv_args_,
                                            "sound-" + std::to_string(++next_player_));
  if (!player->play(sound_target(sound, dir_), sound.name)) {
    error_ = "couldn't start mpv for " + sound.name;
    active_.erase(std::remove(active_.begin(), active_.end(), sound.id), active_.end());
    return;
  }
  players_[sound.id] = std::move(player);
  if (std::find(active_.begin(), active_.end(), sound.id) == active_.end()) {
    active_.push_back(sound.id);
  }
}

void SoundBoard::start_download(const Sound& sound) {
  auto download = std::make_unique<Download>();
  Download* d = download.get();
  const auto final_path = dir_ / sound.file;
  const auto part_path = dir_ / (sound.file + ".part");
  const std::string url = sound_url(sound);

  d->thread = std::jthread([d, final_path, part_path, url] {
    std::error_code ec;
    std::filesystem::create_directories(final_path.parent_path(), ec);
    // Saved under another name first, so a cut-off download never looks finished.
    const pid_t pid = spawn({"curl", "-fsSL", "--retry", "2", "-o", part_path.string(), url});
    d->pid = pid;
    int status = 1;
    if (pid > 0) waitpid(pid, &status, 0);
    d->pid = -1;
    bool ok = pid > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (ok) {
      std::filesystem::rename(part_path, final_path, ec);
      ok = !ec;
    }
    if (!ok) std::filesystem::remove(part_path, ec);
    d->ok = ok;
    d->done = true;
  });
  downloads_[sound.id] = std::move(download);
}

void SoundBoard::update() {
  for (auto it = downloads_.begin(); it != downloads_.end();) {
    if (!it->second->done) {
      ++it;
      continue;
    }
    const std::string id = it->first;
    const bool ok = it->second->ok;
    it = downloads_.erase(it);  // the thread has finished; this joins it

    const auto wanted = waiting_.find(id);
    if (wanted == waiting_.end()) continue;  // turned off while downloading
    if (ok && paused_) continue;             // starts when the break ends
    const Wanted w = wanted->second;
    waiting_.erase(wanted);
    if (ok) {
      start_player(w.sound, w.volume);
    } else {
      error_ = "couldn't download " + w.sound.name + " — check your internet connection";
      active_.erase(std::remove(active_.begin(), active_.end(), id), active_.end());
    }
  }
}

void SoundBoard::turn_off(const std::string& id) {
  players_.erase(id);  // stops its mpv
  waiting_.erase(id);  // a running download still finishes, for next time
  active_.erase(std::remove(active_.begin(), active_.end(), id), active_.end());
}

void SoundBoard::all_off() {
  players_.clear();
  waiting_.clear();
  active_.clear();
  paused_ = false;
}

void SoundBoard::set_volume(const std::string& id, int volume) {
  if (const auto it = players_.find(id); it != players_.end()) it->second->set_volume(volume);
  if (const auto it = waiting_.find(id); it != waiting_.end()) it->second.volume = volume;
}

void SoundBoard::set_pause(bool paused) {
  paused_ = paused;
  for (auto& [id, player] : players_) player->set_pause(paused);
  if (paused) return;
  // Start what finished downloading during the pause.
  for (auto it = waiting_.begin(); it != waiting_.end();) {
    if (downloads_.count(it->first)) {
      ++it;
      continue;
    }
    const Wanted w = it->second;
    it = waiting_.erase(it);
    start_player(w.sound, w.volume);
  }
}

bool SoundBoard::toggle_pause() {
  if (active_.empty()) return false;
  set_pause(!paused_);
  return true;
}

bool SoundBoard::on(const std::string& id) const {
  return std::find(active_.begin(), active_.end(), id) != active_.end();
}

bool SoundBoard::downloading(const std::string& id) const {
  return waiting_.count(id) > 0 && downloads_.count(id) > 0;
}

std::string SoundBoard::take_error() {
  std::string error;
  error.swap(error_);
  return error;
}

}  // namespace potter

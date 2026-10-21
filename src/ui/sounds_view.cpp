#include <algorithm>
#include <cstdlib>
#include <string>

#include "system/player.hpp"
#include "system/sounds.hpp"
#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

// The screen grows with the window, within these limits.
constexpr int kMinWidth = 60;
constexpr int kMaxWidth = 90;
constexpr int kNameWidth = 16;
constexpr int kBarWidth = 20;
// How much ←/→ changes a sound's volume.
constexpr int kVolumeStep = 10;

// "/home/me/.local/share/potter/sounds" -> "~/.local/share/potter/sounds".
std::string short_path(const std::filesystem::path& path) {
  const std::string s = path.string();
  const char* home = std::getenv("HOME");
  if (home && *home && s.starts_with(std::string(home) + "/")) {
    return "~" + s.substr(std::string(home).size());
  }
  return s;
}

}  // namespace

SoundsScreen::SoundsScreen(SoundBoard& board, SoundMix& mix, std::filesystem::path dir)
    : board_(board), mix_(mix), dir_(std::move(dir)) {}

void SoundsScreen::open() {
  done_ = false;
  message_.clear();
  // Re-read the folder, so files you just added show up.
  sounds_ = list_sounds(dir_);
  selected_ = std::clamp(selected_, 0, static_cast<int>(sounds_.size()) - 1);
}

void SoundsScreen::toggle(const Sound& sound) {
  if (board_.on(sound.id)) {
    board_.turn_off(sound.id);
  } else {
    board_.turn_on(sound, mix_.volume(sound.id));
  }
}

void SoundsScreen::change_volume(const Sound& sound, int delta) {
  const int volume = std::clamp(mix_.volume(sound.id) + delta, 0, 100);
  mix_.set_volume(sound.id, volume);
  // Turning the volume up on a sound that's off turns it on, like a slider.
  if (board_.on(sound.id)) {
    board_.set_volume(sound.id, volume);
  } else if (delta > 0) {
    board_.turn_on(sound, volume);
  }
}

void SoundsScreen::bring_back_last_mix() {
  if (mix_.last_mix().empty()) {
    message_ = "no mix yet: turn some sounds on first";
    return;
  }
  for (const std::string& id : mix_.last_mix()) {
    const auto it = std::find_if(sounds_.begin(), sounds_.end(),
                                 [&](const Sound& s) { return s.id == id; });
    if (it != sounds_.end() && !board_.on(id)) board_.turn_on(*it, mix_.volume(id));
  }
}

bool SoundsScreen::handle(const Event& event) {
  message_.clear();
  if (sounds_.empty()) sounds_ = list_sounds(dir_);
  const int last = static_cast<int>(sounds_.size()) - 1;
  const Sound& sound = sounds_[std::clamp(selected_, 0, last)];

  if (event == Event::Character('j') || event == Event::ArrowDown) {
    selected_ = std::min(selected_ + 1, last);
  } else if (event == Event::Character('k') || event == Event::ArrowUp) {
    selected_ = std::max(selected_ - 1, 0);
  } else if (event == Event::Return || event == Event::Character(' ')) {
    toggle(sound);
  } else if (event == Event::ArrowRight || event == Event::Character('l')) {
    change_volume(sound, +kVolumeStep);
  } else if (event == Event::ArrowLeft || event == Event::Character('h')) {
    change_volume(sound, -kVolumeStep);
  } else if (event == Event::Character('x')) {
    board_.all_off();
  } else if (event == Event::Character('r')) {
    bring_back_last_mix();
  } else if (event == Event::Escape || event == Event::Character('b') ||
             event == Event::Character('q')) {
    done_ = true;
  }
  mix_.set_last_mix(board_.active());
  return true;
}

Element SoundsScreen::render(Color accent) const {
  const int width = screen_width(kMinWidth, kMaxWidth);
  // Width available for text: minus the border and the padding.
  const int inner = width - 2 - 2 * kPadX;

  if (!MpvPlayer::available()) {
    return vbox({
               text("Sounds") | bold,
               separator(),
               paragraph("potter plays sounds with mpv, which isn't installed. Install it, "
                         "e.g. sudo pacman -S mpv, sudo apt install mpv or brew install mpv."),
               separator(),
               text("press esc to close") | dim | hcenter,
           }) |
           [&](Element e) { return padded_box(std::move(e), width); };
  }

  Elements rows;
  bool yours_heading = false;
  for (int i = 0; i < static_cast<int>(sounds_.size()); ++i) {
    const Sound& s = sounds_[i];
    if (s.yours && !yours_heading) {
      rows.push_back(text(""));
      rows.push_back(text("Your sounds") | bold);
      yours_heading = true;
    }
    const bool on = board_.on(s.id);
    const int volume = mix_.volume(s.id);

    Element detail;
    if (board_.downloading(s.id)) {
      detail = text("downloading " + format_size(s.size_kb) + "…") | dim;
    } else if (on) {
      detail = hbox({
          gauge(volume / 100.f) | color(accent) | size(WIDTH, EQUAL, kBarWidth),
          text(pad_left(std::to_string(volume) + "%", 6)) | dim,
      });
    } else {
      std::error_code ec;
      const bool missing = !s.generated() && !s.yours && !std::filesystem::exists(dir_ / s.file, ec);
      detail = text(missing ? format_size(s.size_kb) + " download" : "") | dim;
    }

    Element row = hbox({
        text(on ? "  ● " : "  ○ ") | color(on ? accent : Color::Default),
        text(fit(s.name, kNameWidth)) | size(WIDTH, EQUAL, kNameWidth) | (on ? bold : nothing),
        text("  "),
        detail,
        filler(),
    });
    if (i == selected_) row = row | inverted | focus;
    rows.push_back(row);
  }

  std::string playing;
  for (const std::string& id : board_.active()) {
    const auto it = std::find_if(sounds_.begin(), sounds_.end(),
                                 [&](const Sound& s) { return s.id == id; });
    if (it == sounds_.end()) continue;
    playing += (playing.empty() ? "" : " · ") + it->name;
  }
  std::string status = playing.empty() ? "nothing on" : "≋ " + playing;
  if (board_.paused()) status += " · paused for the break";

  Element footer = vbox({
      message_.empty() ? text("enter on/off · ← → volume · r last mix · x all off") | dim
                       : text(message_) | color(accent),
      text(fit("your own sounds: put audio files in " + short_path(dir_), inner)) | dim,
      text("esc close") | dim,
  });

  Element content = vbox({
      hbox({text("Sounds") | bold, filler(), text(fit(status, width / 2)) | dim}),
      separator(),
      text(""),
      vbox(std::move(rows)) | vscroll_indicator | yframe | flex,
      text(""),
      separator(),
      footer,
  });
  return padded_box(std::move(content), width);
}

}  // namespace potter::ui

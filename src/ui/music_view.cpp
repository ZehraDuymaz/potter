#include <algorithm>
#include <filesystem>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include "system/player.hpp"
#include "system/search.hpp"
#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

// The screen grows with the window, within these limits.
constexpr int kMinWidth = 60;
constexpr int kMaxWidth = 120;
constexpr int kPadX = 2;  // spaces inside the border, left and right
// Columns of a search result row.
constexpr int kChannelWidth = 24;
constexpr int kLengthWidth = 8;
constexpr int kGap = 3;

int screen_width() { return std::clamp(Terminal::Size().dimx - 6, kMinWidth, kMaxWidth); }

// Cuts `s` to at most `width` terminal columns, ending with "…" if cut.
std::string fit(const std::string& s, int width) {
  if (string_width(s) <= width) return s;
  std::string out;
  int used = 0;
  for (const std::string& glyph : Utf8ToGlyphs(s)) {
    const int w = string_width(glyph);
    if (used + w > width - 1) break;
    out += glyph;
    used += w;
  }
  return out + "…";
}

std::string pad_left(const std::string& s, int width) {
  const int w = string_width(s);
  return w >= width ? s : std::string(width - w, ' ') + s;
}

// Wraps the screen's content in padding and a rounded border.
Element frame_box(Element content, int width) {
  const std::string pad(kPadX, ' ');
  return vbox({
             text(""),
             hbox({text(pad), std::move(content) | flex, text(pad)}) | flex,
             text(""),
         }) |
         size(WIDTH, EQUAL, width) | borderRounded | center;
}

std::string trim(const std::string& s) {
  const auto start = s.find_first_not_of(" \t");
  if (start == std::string::npos) return "";
  return s.substr(start, s.find_last_not_of(" \t") - start + 1);
}

}  // namespace

MusicScreen::MusicScreen(MpvPlayer& player, YouTubeSearch& search, Favorites& favorites,
                         const Config& config)
    : player_(player), search_(search), favorites_(favorites), config_(config) {
  InputOption option;
  option.multiline = false;
  option.cursor_position = &cursor_;
  option.transform = [](InputState state) { return state.element; };
  input_ = Input(&buffer_, option);
  // Same trick as the task panel: a visible cursor even when the box is
  // empty, rendered through a component so it wins over the list focus.
  field_ = Renderer([this] {
    if (!buffer_.empty()) return input_->Render();
    return hbox({text(" ") | focusCursorBarBlinking, filler()});
  });
}

void MusicScreen::open() {
  done_ = false;
  pending_delete_ = -1;
  message_.clear();
  // Nothing to browse yet? Go straight to typing.
  typing_ = favorites_.list().empty() && search_.status().results.empty();
  if (typing_) {
    buffer_.clear();
    cursor_ = 0;
  }
}

std::vector<MusicScreen::Item> MusicScreen::items() const {
  std::vector<Item> list;
  const auto status = search_.status();
  for (std::size_t i = 0; i < status.results.size(); ++i) list.push_back({true, i});
  for (std::size_t i = 0; i < favorites_.list().size(); ++i) list.push_back({false, i});
  return list;
}

void MusicScreen::play_item(const Item& item) {
  if (item.is_result) {
    const auto results = search_.status().results;
    if (item.index < results.size()) {
      player_.play(results[item.index].url(), results[item.index].title);
    }
  } else if (item.index < favorites_.list().size()) {
    const Favorite& f = favorites_.list()[item.index];
    player_.play(f.target, f.title);
  }
}

void MusicScreen::submit() {
  const std::string input = trim(buffer_);
  typing_ = false;
  if (input.empty()) return;

  switch (classify_input(input)) {
    case InputKind::Search:
      if (!YouTubeSearch::available()) {
        message_ = "searching YouTube needs yt-dlp — install it to search";
        return;
      }
      search_.start(input, config_.music_search_results);
      selected_ = 0;
      break;
    case InputKind::Url:
      player_.play(input, input);
      break;
    case InputKind::File: {
      const std::string path = expand_home(input);
      std::error_code ec;
      if (!std::filesystem::exists(path, ec)) {
        message_ = "not found: " + path;
        return;
      }
      player_.play(path, std::filesystem::path(path).filename().string());
      break;
    }
  }
  buffer_.clear();
  cursor_ = 0;
}

void MusicScreen::toggle_favorite() {
  const PlayerState state = player_.state();
  if (!state.has_media() || state.path.empty()) {
    message_ = "play something first, then press f to save it";
    return;
  }
  if (favorites_.remove_target(state.path)) {
    message_ = "removed from favorites";
  } else {
    favorites_.add(player_.display_title(), state.path);
    message_ = "★ added to favorites";
  }
}

bool MusicScreen::handle(const Event& event) {
  if (typing_) {
    if (event == Event::Return) {
      submit();
    } else if (event == Event::Escape) {
      typing_ = false;
    } else {
      input_->OnEvent(event);
    }
    return true;
  }

  const int confirm_delete = pending_delete_;
  pending_delete_ = -1;
  message_.clear();

  const auto list = items();
  if (!list.empty()) selected_ = std::clamp(selected_, 0, static_cast<int>(list.size()) - 1);

  if (event == Event::Character('/') || event == Event::Character('i') ||
      event == Event::Character('a')) {
    typing_ = true;
    buffer_.clear();
    cursor_ = 0;
  } else if (event == Event::Character('j') || event == Event::ArrowDown) {
    if (!list.empty()) selected_ = std::min(selected_ + 1, static_cast<int>(list.size()) - 1);
  } else if (event == Event::Character('k') || event == Event::ArrowUp) {
    selected_ = std::max(selected_ - 1, 0);
  } else if (event == Event::Return) {
    if (!list.empty()) play_item(list[selected_]);
  } else if (event == Event::Character('p') || event == Event::Character(' ')) {
    player_.toggle_pause();
  } else if (event == Event::Character('+') || event == Event::Character('=')) {
    player_.change_volume(+5);
  } else if (event == Event::Character('-')) {
    player_.change_volume(-5);
  } else if (event == Event::Character('f')) {
    toggle_favorite();
  } else if (event == Event::Character('x')) {
    player_.stop();
  } else if (event == Event::Character('d')) {
    if (!list.empty() && !list[selected_].is_result) {
      const int index = static_cast<int>(list[selected_].index);
      if (confirm_delete == index) {
        favorites_.remove(static_cast<std::size_t>(index));
      } else {
        pending_delete_ = index;
      }
    }
  } else if (event == Event::Escape || event == Event::Character('m') ||
             event == Event::Character('q')) {
    done_ = true;
  }
  return true;
}

Element MusicScreen::render(Color accent) const {
  const int width = screen_width();
  // Width available for text: minus the border and the padding.
  const int inner = width - 2 - 2 * kPadX;

  if (!MpvPlayer::available()) {
    return vbox({
               text("Music") | bold,
               separator(),
               paragraph("potter plays music with mpv, which isn't installed. Install it "
                         "(and yt-dlp for YouTube), e.g.:"),
               text(""),
               text("  sudo pacman -S mpv yt-dlp      # Arch / CachyOS") | color(accent),
               text("  sudo apt install mpv yt-dlp    # Debian / Ubuntu") | color(accent),
               text("  brew install mpv yt-dlp        # macOS") | color(accent),
               separator(),
               text("press esc to close") | dim | hcenter,
           }) |
           [&](Element e) { return frame_box(std::move(e), width); };
  }

  const PlayerState state = player_.state();
  const auto status = search_.status();

  // Now playing.
  Elements now;
  if (state.has_media()) {
    const bool favorite = favorites_.contains(state.path);
    now.push_back(hbox({
        text(state.paused ? "⏸  " : "▶  ") | color(accent),
        paragraph(player_.display_title()) | bold | flex,
        text(favorite ? " ★" : "") | color(accent),
    }));
    const double pos = state.time_pos.value_or(0);
    std::string time = format_clock(pos);
    Element bar = text("");
    if (state.duration && *state.duration > 0) {
      time += " / " + format_clock(*state.duration);
      bar = gauge(static_cast<float>(pos / *state.duration)) | color(accent) | flex;
    } else {
      time += " · live";
    }
    now.push_back(hbox({text("   " + time + "  ") | dim, bar}));
  } else if (player_.loading()) {
    now.push_back(text("…  loading " + player_.display_title()) | dim);
    now.push_back(text("   YouTube links take a few seconds") | dim);
  } else {
    now.push_back(text("   nothing playing") | dim);
    now.push_back(text(""));
  }
  if (!state.error.empty()) now.push_back(text("   " + state.error) | color(Color::Red));

  // The search / link box.
  Element box = typing_ ? hbox({text("search or paste a link: ") | bold, field_->Render() | flex})
                        : text("press / to search YouTube, paste a link or a file path") | dim;

  // Results and favorites share one selectable list.
  const auto list = items();
  const int selected = list.empty() ? -1 : std::clamp(selected_, 0, static_cast<int>(list.size()) - 1);
  Elements rows;
  auto add_row = [&](Element row, int index) {
    if (index == selected && !typing_) row = row | inverted | focus;
    rows.push_back(row);
  };

  if (status.searching) {
    rows.push_back(text("searching “" + status.query + "”…") | dim);
  } else if (!status.query.empty()) {
    rows.push_back(text("Results for “" + status.query + "”") | bold);
    if (!status.error.empty()) rows.push_back(text("  " + status.error) | dim);
  }
  // Results in columns: title · channel · length, with gaps between them.
  const int title_width = std::max(10, inner - 2 - kGap - kChannelWidth - kGap - kLengthWidth);
  int index = 0;
  for (const SearchResult& r : status.results) {
    const std::string length = r.duration_seconds ? format_clock(*r.duration_seconds) : "live";
    add_row(hbox({
                text("  "),
                text(fit(r.title, title_width)) | size(WIDTH, EQUAL, title_width),
                text(std::string(kGap, ' ')),
                text(fit(r.channel, kChannelWidth)) | dim | size(WIDTH, EQUAL, kChannelWidth),
                text(std::string(kGap, ' ')),
                text(pad_left(length, kLengthWidth)) | dim,
            }),
            index++);
  }

  if (!rows.empty()) rows.push_back(text(""));
  rows.push_back(text("Favorites") | bold);
  if (favorites_.list().empty()) {
    rows.push_back(text("  press f while something plays to save it here") | dim);
  }
  for (const Favorite& f : favorites_.list()) {
    add_row(hbox({text("  ★ ") | color(accent), text(fit(f.title, inner - 4)) | flex}), index++);
  }

  // Footer: a message, a delete prompt, or the key hints.
  Element footer;
  if (pending_delete_ >= 0 && pending_delete_ < static_cast<int>(favorites_.list().size())) {
    footer = vbox({
        text("remove \"" + favorites_.list()[pending_delete_].title + "\" from favorites?") |
            color(Color::Red),
        text("press d again · any other key cancels") | dim,
    });
  } else if (typing_) {
    footer = vbox({
        text("enter search / play · esc cancel") | dim,
        text("tip: links (https://…) and paths (~/Music/…) play directly") | dim,
    });
  } else {
    footer = vbox({
        text(message_.empty() ? "/ search · enter play · p pause · +/- volume · f favorite"
                              : message_) |
            (message_.empty() ? dim : color(accent)),
        text("x stop · dd remove favorite · esc close") | dim,
    });
  }

  const int volume = static_cast<int>(state.connected ? state.volume : player_.volume());
  Element content = vbox({
      hbox({text("Music") | bold, filler(), text("vol " + std::to_string(volume) + "%") | dim}),
      separator(),
      text(""),
      vbox(std::move(now)),
      text(""),
      separator(),
      text(""),
      box,
      text(""),
      separator(),
      text(""),
      vbox(std::move(rows)) | vscroll_indicator | yframe | flex,
      text(""),
      separator(),
      footer,
  });
  return frame_box(std::move(content), width);
}

}  // namespace potter::ui

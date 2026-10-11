#include <array>
#include <cstdio>
#include <string>

#include <ftxui/screen/terminal.hpp>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

// 3x5 glyphs; each '#' is drawn as two block characters so they look square.
constexpr std::array<std::array<const char*, 5>, 11> kGlyphs = {{
    {"###", "#.#", "#.#", "#.#", "###"},  // 0
    {".#.", "##.", ".#.", ".#.", "###"},  // 1
    {"###", "..#", "###", "#..", "###"},  // 2
    {"###", "..#", "###", "..#", "###"},  // 3
    {"#.#", "#.#", "###", "..#", "..#"},  // 4
    {"###", "#..", "###", "..#", "###"},  // 5
    {"###", "#..", "###", "#.#", "###"},  // 6
    {"###", "..#", "..#", "..#", "..#"},  // 7
    {"###", "#.#", "###", "#.#", "###"},  // 8
    {"###", "#.#", "###", "..#", "###"},  // 9
    {".", "#", ".", "#", "."},            // :
}};

Element big_text(const std::string& digits) {
  Elements rows;
  for (int row = 0; row < 5; ++row) {
    std::string line;
    for (std::size_t i = 0; i < digits.size(); ++i) {
      const char c = digits[i];
      const int index = c == ':' ? 10 : c - '0';
      if (i > 0) line += "  ";
      for (const char* p = kGlyphs[index][row]; *p; ++p) {
        line += *p == '#' ? "██" : "  ";
      }
    }
    rows.push_back(text(line));
  }
  return vbox(std::move(rows));
}

std::string format_time(Timer::Duration remaining) {
  // Round up so a fresh 25 minute timer shows 25:00 and the last second 00:01.
  const auto total = std::chrono::ceil<std::chrono::seconds>(remaining).count();
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02lld:%02lld", static_cast<long long>(total / 60),
                static_cast<long long>(total % 60));
  return buf;
}

}  // namespace

Color phase_color(Phase phase, const Config& config) {
  if (config.theme == ThemeMode::Custom) {
    const std::string& hex = phase == Phase::Focus        ? config.focus_color
                             : phase == Phase::ShortBreak ? config.short_break_color
                                                          : config.long_break_color;
    if (auto rgb = parse_hex_color(hex)) return Color::RGB(rgb->r, rgb->g, rgb->b);
  }
  // Palette colors come from the terminal's theme, so they match the rice.
  switch (phase) {
    case Phase::Focus: return Color::Red;
    case Phase::ShortBreak: return Color::Green;
    case Phase::LongBreak: return Color::Blue;
  }
  return Color::Default;
}

Element timer_view(const Timer& timer, const Config& config,
                   Timer::Clock::time_point now) {
  const Color accent = phase_color(timer.phase(), config);
  const auto remaining = timer.remaining(now);
  const std::string clock = format_time(remaining);

  const auto term = Terminal::Size();
  Element countdown = term.dimx >= 50 && term.dimy >= 14
                          ? big_text(clock)
                          : text(clock) | bold;

  const double length = static_cast<double>(timer.phase_length().count());
  const float progress =
      length > 0 ? static_cast<float>(1.0 - remaining.count() / length) : 0.f;

  std::string dots;
  const int sessions = timer.settings().sessions_before_long_break;
  for (int i = 0; i < sessions; ++i) {
    if (i > 0) dots += ' ';
    dots += i < timer.cycle_position() ? "●" : "○";
  }

  std::string state = timer.running()     ? "running"
                      : timer.untouched() ? "ready · press space"
                                          : "paused";

  return vbox({
             text(phase_name(timer.phase())) | bold | color(accent) | hcenter,
             text(""),
             countdown | color(accent) | hcenter,
             text(""),
             gauge(progress) | color(accent) | size(WIDTH, EQUAL, 40) | hcenter,
             text(""),
             hbox({text(dots) | color(accent),
                   text("  " + std::to_string(timer.completed_focus()) + " today") | dim}) |
                 hcenter,
             text(state) | dim | hcenter,
             text(""),
             text("space start/pause · r reset · n skip · s settings · ? help · q quit") |
                 dim | hcenter,
         }) |
         center;
}

Element help_view() {
  auto row = [](const char* key, const char* action) {
    return hbox({text(key) | bold | size(WIDTH, EQUAL, 10), text(action)});
  };
  return vbox({
             text("potter keys") | bold | hcenter,
             separator(),
             row("space", "start / pause"),
             row("r", "reset current phase"),
             row("n", "skip to next phase"),
             row("s", "settings"),
             row("?", "this help"),
             row("q", "quit"),
             separator(),
             text("press any key to close") | dim | hcenter,
         }) |
         borderRounded | center;
}

}  // namespace potter::ui

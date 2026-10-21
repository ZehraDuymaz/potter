#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

namespace {

enum Row {
  kFocus,
  kShortBreak,
  kLongBreak,
  kSessions,
  kGoal,
  kDayStart,
  kAutoBreaks,
  kAutoFocus,
  kDesktop,
  kBell,
  kMusicBreaks,
  kTheme,
  kFocusColor,
  kShortColor,
  kLongColor,
  kRowCount,
};

const char* row_label(int row) {
  switch (row) {
    case kFocus: return "Focus length";
    case kShortBreak: return "Short break length";
    case kLongBreak: return "Long break length";
    case kSessions: return "Sessions before long break";
    case kGoal: return "Daily focus goal";
    case kDayStart: return "New day starts at";
    case kAutoBreaks: return "Auto-start breaks";
    case kAutoFocus: return "Auto-start focus";
    case kDesktop: return "Desktop notifications";
    case kBell: return "Terminal bell";
    case kMusicBreaks: return "Pause audio during breaks";
    case kTheme: return "Colors";
    case kFocusColor: return "Focus color";
    case kShortColor: return "Short break color";
    case kLongColor: return "Long break color";
  }
  return "";
}

void step_minutes(std::chrono::minutes& value, int delta, int max) {
  value = std::chrono::minutes(std::clamp<int>(value.count() + delta, 1, max));
}

// Works for both Config and const Config.
template <typename C>
auto* color_field(C& config, int row) {
  switch (row) {
    case kFocusColor: return &config.focus_color;
    case kShortColor: return &config.short_break_color;
    default: return &config.long_break_color;
  }
}

Element swatch(const std::string& hex) {
  auto rgb = parse_hex_color(hex);
  if (!rgb) return text("  ");
  return text("██") | color(Color::RGB(rgb->r, rgb->g, rgb->b));
}

}  // namespace

void SettingsMenu::open(const Config& config) {
  draft_ = config;
  selected_ = 0;
  done_ = false;
  editing_ = false;
  error_.clear();
}

bool SettingsMenu::handle(const Event& event) {
  if (editing_) {
    if (event == Event::Return) {
      if (parse_hex_color(edit_buffer_)) {
        std::string hex = edit_buffer_;
        if (hex.front() != '#') hex.insert(hex.begin(), '#');
        *color_field(draft_, selected_) = hex;
        editing_ = false;
        error_.clear();
      } else {
        error_ = "use the form #rrggbb";
      }
    } else if (event == Event::Escape) {
      editing_ = false;
      error_.clear();
    } else if (event == Event::Backspace) {
      if (!edit_buffer_.empty()) edit_buffer_.pop_back();
    } else if (event.is_character() && event.character().size() == 1) {
      const char c = event.character()[0];
      if ((std::isxdigit(static_cast<unsigned char>(c)) || c == '#') &&
          edit_buffer_.size() < 7) {
        edit_buffer_ += c;
      }
    }
    return true;
  }

  if (event == Event::ArrowDown || event == Event::Character('j')) {
    selected_ = (selected_ + 1) % kRowCount;
  } else if (event == Event::ArrowUp || event == Event::Character('k')) {
    selected_ = (selected_ + kRowCount - 1) % kRowCount;
  } else if (event == Event::ArrowRight || event == Event::Character('l')) {
    adjust(+1);
  } else if (event == Event::ArrowLeft || event == Event::Character('h')) {
    adjust(-1);
  } else if (event == Event::Character('L')) {
    adjust(+5);
  } else if (event == Event::Character('H')) {
    adjust(-5);
  } else if (event == Event::Return || event == Event::Character(' ')) {
    activate();
  } else if (event == Event::Escape || event == Event::Character('s') ||
             event == Event::Character('q')) {
    done_ = true;
  }
  return true;
}

void SettingsMenu::adjust(int delta) {
  auto& timer = draft_.timer;
  switch (selected_) {
    case kFocus: step_minutes(timer.focus, delta, 180); break;
    case kShortBreak: step_minutes(timer.short_break, delta, 60); break;
    case kLongBreak: step_minutes(timer.long_break, delta, 120); break;
    case kSessions:
      timer.sessions_before_long_break =
          std::clamp(timer.sessions_before_long_break + delta, 1, 12);
      break;
    case kGoal:  // in steps of 5 minutes
      draft_.daily_goal_minutes = std::clamp(draft_.daily_goal_minutes + delta * 5, 5, 720);
      break;
    case kDayStart: draft_.day_start_hour = std::clamp(draft_.day_start_hour + delta, 0, 12); break;
    case kFocusColor:
    case kShortColor:
    case kLongColor: break;  // edited as text, not stepped
    default: activate(); break;  // toggles flip either way
  }
}

void SettingsMenu::activate() {
  switch (selected_) {
    case kAutoBreaks: draft_.timer.auto_start_breaks ^= true; break;
    case kAutoFocus: draft_.timer.auto_start_focus ^= true; break;
    case kDesktop: draft_.desktop_notifications ^= true; break;
    case kBell: draft_.terminal_bell ^= true; break;
    case kMusicBreaks: draft_.pause_music_on_breaks ^= true; break;
    case kTheme:
      draft_.theme =
          draft_.theme == ThemeMode::Terminal ? ThemeMode::Custom : ThemeMode::Terminal;
      break;
    case kFocusColor:
    case kShortColor:
    case kLongColor:
      editing_ = true;
      edit_buffer_ = *color_field(draft_, selected_);
      // Editing a custom color only makes sense with custom colors on.
      draft_.theme = ThemeMode::Custom;
      break;
    default: break;
  }
}

Element SettingsMenu::render() const {
  auto on_off = [](bool v) { return std::string(v ? "on" : "off"); };
  auto minutes = [](std::chrono::minutes m) {
    return "‹ " + std::to_string(m.count()) + " min ›";
  };
  const auto& timer = draft_.timer;
  const bool custom = draft_.theme == ThemeMode::Custom;

  Elements rows;
  for (int row = 0; row < kRowCount; ++row) {
    Element value;
    switch (row) {
      case kFocus: value = text(minutes(timer.focus)); break;
      case kShortBreak: value = text(minutes(timer.short_break)); break;
      case kLongBreak: value = text(minutes(timer.long_break)); break;
      case kSessions:
        value = text("‹ " + std::to_string(timer.sessions_before_long_break) + " ›");
        break;
      case kGoal:
        value = text("‹ " + format_focus(std::chrono::minutes(draft_.daily_goal_minutes)) + " ›");
        break;
      case kDayStart: {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "‹ %02d:00 ›", draft_.day_start_hour);
        value = text(buf);
        break;
      }
      case kAutoBreaks: value = text(on_off(timer.auto_start_breaks)); break;
      case kAutoFocus: value = text(on_off(timer.auto_start_focus)); break;
      case kDesktop: value = text(on_off(draft_.desktop_notifications)); break;
      case kBell: value = text(on_off(draft_.terminal_bell)); break;
      case kMusicBreaks: value = text(on_off(draft_.pause_music_on_breaks)); break;
      case kTheme: value = text(custom ? "custom" : "terminal palette"); break;
      default: {
        const std::string& hex = *color_field(draft_, row);
        if (editing_ && row == selected_) {
          value = hbox({swatch(edit_buffer_), text(" " + edit_buffer_ + "▏")});
        } else {
          value = hbox({swatch(hex), text(" " + hex)});
          if (!custom) value = value | dim;
        }
      }
    }

    Element line = hbox({text(row_label(row)) | size(WIDTH, EQUAL, 30), value});
    if (row == selected_) line = line | inverted;
    rows.push_back(line);
    if (row == kSessions || row == kDayStart || row == kAutoFocus || row == kMusicBreaks) {
      rows.push_back(text(""));
    }
  }

  rows.push_back(separator());
  if (!error_.empty()) {
    rows.push_back(text(error_) | color(Color::Red));
  } else if (editing_) {
    rows.push_back(text("type a hex color · enter confirm · esc cancel") | dim);
  } else {
    rows.push_back(text("↑↓ move · ←→ change (H/L ±5) · enter toggle/edit · esc save") |
                   dim);
  }

  return vbox({text("Settings") | bold | hcenter, separator(), vbox(std::move(rows))}) |
         borderRounded | center;
}

}  // namespace potter::ui

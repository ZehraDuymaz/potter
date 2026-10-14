#include <algorithm>
#include <string>

#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include "ui/views.hpp"

using namespace ftxui;

namespace potter::ui {

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

int screen_width(int min_width, int max_width) {
  return std::clamp(Terminal::Size().dimx - 6, min_width, max_width);
}

Element padded_box(Element content, int width) {
  const std::string pad(kPadX, ' ');
  return vbox({
             text(""),
             hbox({text(pad), std::move(content) | flex, text(pad)}) | flex,
             text(""),
         }) |
         size(WIDTH, EQUAL, width) | borderRounded | center;
}

}  // namespace potter::ui

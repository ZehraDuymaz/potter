#pragma once

#include <string>

namespace potter {

// Shows a desktop notification via notify-send (osascript on macOS).
// Fire-and-forget: it never blocks the UI, and does nothing if the program
// isn't installed.
void send_notification(const std::string& title, const std::string& body);

}  // namespace potter

#include <signal.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "app.hpp"
#include "core/config.hpp"
#include "core/history.hpp"
#include "core/music.hpp"
#include "core/tasks.hpp"

namespace {

constexpr const char* kUsage =
    "usage: potter [--version] [--help]\n"
    "\n"
    "A lightweight Pomodoro timer for the terminal.\n"
    "Press ? inside potter to see the keys.\n";

}  // namespace

int main(int argc, char** argv) {
  using namespace ftxui;

  if (argc > 1) {
    const std::string_view arg = argv[1];
    if (arg == "--version" || arg == "-v") {
      std::printf("potter %s\n", POTTER_VERSION);
      return 0;
    }
    if (arg == "--help" || arg == "-h") {
      std::fputs(kUsage, stdout);
      return 0;
    }
    std::fprintf(stderr, "potter: unknown option '%s'\n\n%s", argv[1], kUsage);
    return 2;
  }

  // If mpv dies while potter is sending it a command, the write should just
  // fail instead of killing potter with SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

  const auto path = potter::config_path();
  std::string error;
  potter::Config config = potter::load_config(path, &error);

  // Fullscreen uses the alternate screen, so the shell is restored on exit.
  // Nothing sets a background color, so terminal transparency shows through.
  auto screen = ScreenInteractive::Fullscreen();
  // potter quits on Ctrl-C itself, so it can save before exiting.
  screen.ForceHandleCtrlC(false);
  const potter::Paths paths{path, potter::tasks_path(), potter::history_path(),
                            potter::favorites_path(), potter::resume_points_path()};
  potter::App app(std::move(config), paths, screen.ExitLoopClosure());
  app.set_status(error);

  auto root = CatchEvent(Renderer([&] { return app.render(); }),
                         [&](const Event& event) { return app.on_event(event); });

  // Wake the UI a few times per second to advance the timer and redraw.
  std::jthread ticker([&](std::stop_token stop) {
    while (!stop.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      screen.Post([&] { app.tick(); });
      screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(root);
  ticker.request_stop();
  ticker.join();
  app.shutdown();
  return 0;
}

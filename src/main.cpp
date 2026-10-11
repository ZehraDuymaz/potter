#include <chrono>
#include <string>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "app.hpp"
#include "config.hpp"

int main() {
  using namespace ftxui;

  const auto path = potter::config_path();
  std::string error;
  potter::Config config = potter::load_config(path, &error);

  // Fullscreen uses the alternate screen, so the shell is restored on exit.
  // Nothing sets a background color, so terminal transparency shows through.
  auto screen = ScreenInteractive::Fullscreen();
  potter::App app(std::move(config), path, screen.ExitLoopClosure());
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
  return 0;
}

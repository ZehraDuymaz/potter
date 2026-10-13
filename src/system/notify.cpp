#include "system/notify.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <thread>

extern char** environ;

namespace potter {

void send_notification(const std::string& title, const std::string& body) {
  // Arguments are passed directly (no shell), so the text needs no escaping.
  const char* argv[] = {"notify-send", "--app-name=potter", title.c_str(),
                        body.c_str(), nullptr};

  // Silence notify-send's output so it can't draw over the TUI.
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

  pid_t pid = 0;
  const int rc = posix_spawnp(&pid, "notify-send", &actions, nullptr,
                              const_cast<char* const*>(argv), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (rc != 0) return;

  // Reap the child in the background so it doesn't linger as a zombie.
  std::thread([pid] { waitpid(pid, nullptr, 0); }).detach();
}

}  // namespace potter

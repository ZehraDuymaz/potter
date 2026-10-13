#include "system/process.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>

extern char** environ;

namespace potter {

bool in_path(const std::string& name) {
  const char* path = std::getenv("PATH");
  if (!path) return false;
  std::istringstream dirs(path);
  std::string dir;
  while (std::getline(dirs, dir, ':')) {
    if (dir.empty()) continue;
    const auto candidate = std::filesystem::path(dir) / name;
    if (access(candidate.c_str(), X_OK) == 0) return true;
  }
  return false;
}

pid_t spawn(const std::vector<std::string>& args, int stdout_fd) {
  if (args.empty()) return -1;
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  if (stdout_fd >= 0) {
    posix_spawn_file_actions_adddup2(&actions, stdout_fd, STDOUT_FILENO);
  } else {
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
  }
  // Keep the child's chatter off potter's screen.
  posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

  pid_t pid = -1;
  const int rc = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  return rc == 0 ? pid : -1;
}

}  // namespace potter

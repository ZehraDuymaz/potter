#pragma once

#include <sys/types.h>

#include <string>
#include <vector>

namespace potter {

// Whether an executable with this name is on $PATH.
bool in_path(const std::string& name);

// Starts a program (looked up on $PATH) with no shell involved, so arguments
// need no escaping. stdin and stderr go to /dev/null; stdout goes to
// `stdout_fd` if given, otherwise /dev/null too. Returns the pid, or -1.
pid_t spawn(const std::vector<std::string>& args, int stdout_fd = -1);

}  // namespace potter

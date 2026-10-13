#include "system/search.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>

#include "system/process.hpp"

namespace potter {

YouTubeSearch::~YouTubeSearch() { cancel(); }

bool YouTubeSearch::available() { return in_path("yt-dlp"); }

void YouTubeSearch::cancel() {
  {
    std::lock_guard lock(mutex_);
    cancelled_ = true;
    if (pid_ > 0) kill(pid_, SIGTERM);
  }
  if (worker_.joinable()) worker_.join();
  std::lock_guard lock(mutex_);
  cancelled_ = false;
}

void YouTubeSearch::start(std::string query, int count) {
  cancel();
  count = std::clamp(count, 1, 20);
  {
    std::lock_guard lock(mutex_);
    status_ = Status{};
    status_.searching = true;
    status_.query = query;
  }

  worker_ = std::jthread([this, query, count] {
    int out[2];
    if (pipe(out) != 0) {
      std::lock_guard lock(mutex_);
      status_.searching = false;
      status_.error = "search failed";
      return;
    }
    fcntl(out[0], F_SETFD, FD_CLOEXEC);
    fcntl(out[1], F_SETFD, FD_CLOEXEC);

    const pid_t pid = spawn({"yt-dlp", "--flat-playlist", "--no-warnings", "--print",
                             search_print_template(),
                             "ytsearch" + std::to_string(count) + ":" + query},
                            out[1]);
    close(out[1]);
    {
      std::lock_guard lock(mutex_);
      pid_ = pid;
      // cancel() may have run before the pid was known.
      if (cancelled_ && pid > 0) kill(pid, SIGTERM);
    }

    std::string output;
    char buf[4096];
    for (ssize_t n; pid > 0 && (n = read(out[0], buf, sizeof(buf))) > 0;) {
      output.append(buf, static_cast<std::size_t>(n));
    }
    close(out[0]);
    int wstatus = 0;
    if (pid > 0) waitpid(pid, &wstatus, 0);

    std::vector<SearchResult> results;
    std::size_t start = 0;
    for (std::size_t nl; (nl = output.find('\n', start)) != std::string::npos; start = nl + 1) {
      if (auto r = parse_search_line(std::string_view(output).substr(start, nl - start))) {
        results.push_back(std::move(*r));
      }
    }

    std::lock_guard lock(mutex_);
    pid_ = -1;
    status_.searching = false;
    status_.results = std::move(results);
    if (pid < 0) {
      status_.error = "couldn't run yt-dlp";
    } else if (status_.results.empty()) {
      const bool ok = WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
      status_.error = ok ? "no results" : "search failed — check your internet or update yt-dlp";
    }
  });
}

YouTubeSearch::Status YouTubeSearch::status() const {
  std::lock_guard lock(mutex_);
  return status_;
}

}  // namespace potter

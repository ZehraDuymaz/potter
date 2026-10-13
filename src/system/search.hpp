#pragma once

#include <sys/types.h>

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/music.hpp"

namespace potter {

// Searches YouTube with yt-dlp in a background thread, so the UI never
// waits for the network. A new search cancels the one still running.
class YouTubeSearch {
 public:
  YouTubeSearch() = default;
  ~YouTubeSearch();
  YouTubeSearch(const YouTubeSearch&) = delete;
  YouTubeSearch& operator=(const YouTubeSearch&) = delete;

  // Whether yt-dlp is installed.
  static bool available();

  void start(std::string query, int count);

  struct Status {
    bool searching = false;
    std::string query;
    std::vector<SearchResult> results;
    std::string error;
  };
  Status status() const;

 private:
  void cancel();

  mutable std::mutex mutex_;
  Status status_;
  pid_t pid_ = -1;
  bool cancelled_ = false;  // set by cancel(), checked once yt-dlp is started
  std::jthread worker_;
};

}  // namespace potter

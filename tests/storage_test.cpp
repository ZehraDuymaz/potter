// Unit tests for XDG paths and safe file writes. Same style as timer_test.cpp.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "core/storage.hpp"

using namespace potter;

static int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

static const auto kDir = std::filesystem::temp_directory_path() / "potter-storage-test";

static std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

// Sets an environment variable for one test and puts the old value back
// afterwards, so the tests don't leak into each other.
class ScopedEnv {
 public:
  ScopedEnv(const char* name, const char* value) : name_(name) {
    if (const char* old = std::getenv(name)) old_ = old;
    if (value) {
      setenv(name, value, 1);
    } else {
      unsetenv(name);
    }
  }
  ~ScopedEnv() {
    if (old_) {
      setenv(name_, old_->c_str(), 1);
    } else {
      unsetenv(name_);
    }
  }
  ScopedEnv(const ScopedEnv&) = delete;
  ScopedEnv& operator=(const ScopedEnv&) = delete;

 private:
  const char* name_;
  std::optional<std::string> old_;
};

static void xdg_dir_follows_the_environment() {
  ScopedEnv home("HOME", "/home/tester");
  {
    ScopedEnv data("XDG_DATA_HOME", "/custom/data");
    CHECK(xdg_dir("XDG_DATA_HOME", ".local/share") == "/custom/data");
  }
  {
    ScopedEnv data("XDG_DATA_HOME", nullptr);
    CHECK(xdg_dir("XDG_DATA_HOME", ".local/share") == "/home/tester/.local/share");
  }
  {
    // The XDG spec says an empty value counts as unset.
    ScopedEnv data("XDG_DATA_HOME", "");
    CHECK(xdg_dir("XDG_DATA_HOME", ".local/share") == "/home/tester/.local/share");
  }
  {
    // Without HOME either, it falls back to the current directory.
    ScopedEnv data("XDG_DATA_HOME", nullptr);
    ScopedEnv no_home("HOME", nullptr);
    CHECK(xdg_dir("XDG_DATA_HOME", ".local/share") == "./.local/share");
  }
}

static void writes_and_creates_directories() {
  std::filesystem::remove_all(kDir);
  const auto path = kDir / "a" / "b" / "file.toml";
  const std::string content = "line one\nline two\n\nütf-8 ✓\n";

  std::string error;
  CHECK(write_atomically(path, content, &error));
  CHECK(error.empty());
  CHECK(read_file(path) == content);

  auto tmp = path;
  tmp += ".tmp";
  CHECK(!std::filesystem::exists(tmp));  // the temp file was renamed away
}

static void replaces_the_whole_file() {
  std::filesystem::remove_all(kDir);
  const auto path = kDir / "file.toml";

  CHECK(write_atomically(path, "a long first version of the file\n"));
  CHECK(write_atomically(path, "short\n"));
  CHECK(read_file(path) == "short\n");  // nothing left over from the first one

  CHECK(write_atomically(path, ""));
  CHECK(std::filesystem::exists(path));
  CHECK(read_file(path).empty());
}

static void reports_failures() {
  std::filesystem::remove_all(kDir);
  std::filesystem::create_directories(kDir);

  // The parent "directory" is a file, so nothing can be written under it.
  std::ofstream(kDir / "not-a-dir") << "x";
  std::string error;
  CHECK(!write_atomically(kDir / "not-a-dir" / "file.toml", "content", &error));
  CHECK(!error.empty());

  // The target is a directory, so the rename fails and the directory stays.
  const auto dir_target = kDir / "taken";
  std::filesystem::create_directories(dir_target);
  error.clear();
  CHECK(!write_atomically(dir_target, "content", &error));
  CHECK(error.find("could not save") != std::string::npos);
  CHECK(std::filesystem::is_directory(dir_target));

  // Passing no error string is allowed.
  CHECK(!write_atomically(dir_target, "content"));
}

int main() {
  xdg_dir_follows_the_environment();
  writes_and_creates_directories();
  replaces_the_whole_file();
  reports_failures();
  std::filesystem::remove_all(kDir);

  if (failures == 0) std::printf("all storage tests passed\n");
  return failures == 0 ? 0 : 1;
}

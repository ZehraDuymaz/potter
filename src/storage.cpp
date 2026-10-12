#include "storage.hpp"

#include <cstdlib>
#include <fstream>
#include <system_error>

namespace potter {

std::filesystem::path xdg_dir(const char* env_var, const char* fallback) {
  if (const char* dir = std::getenv(env_var); dir && *dir) {
    return std::filesystem::path(dir);
  }
  const char* home = std::getenv("HOME");
  return std::filesystem::path(home ? home : ".") / fallback;
}

bool write_atomically(const std::filesystem::path& path, const std::string& content,
                      std::string* error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);

  auto tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary);
    out << content;
    if (!out) {
      if (error) *error = "could not write " + tmp.string();
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    if (error) *error = "could not save " + path.string() + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace potter

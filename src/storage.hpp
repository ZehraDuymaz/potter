#pragma once

#include <filesystem>
#include <string>

namespace potter {

// An XDG base directory: $env_var if set, otherwise ~/<fallback>.
// For example xdg_dir("XDG_DATA_HOME", ".local/share").
std::filesystem::path xdg_dir(const char* env_var, const char* fallback);

// Writes `content` to a temp file next to `path`, then renames it into
// place, so a crash never leaves a half-written file. Creates parent
// directories. Returns false and fills `error` on failure.
bool write_atomically(const std::filesystem::path& path, const std::string& content,
                      std::string* error = nullptr);

}  // namespace potter

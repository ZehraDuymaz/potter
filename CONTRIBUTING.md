# Contributing to potter

Thanks for wanting to help! Bug reports, ideas and pull requests are all welcome.

## Reporting a bug or suggesting an idea

Open an [issue](https://github.com/ZehraDuymaz/potter/issues). For a bug, these help a lot:

- what you did, what you expected and what happened instead
- your potter version (`potter --version`), OS and terminal
- for music problems: your `mpv --version` and `yt-dlp --version`, and whether the commands in the README's [Troubleshooting](README.md#troubleshooting) section work

For a bigger change or a new feature, please open an issue first so we can agree on it before you spend time on the code.

## Building and testing

You need a C++20 compiler (GCC 11+ or Clang 14+) and CMake 3.20+. See [Requirements](README.md#requirements) for the packages on your distro.

```sh
git clone https://github.com/ZehraDuymaz/potter.git
cd potter
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/potter
```

The first configure downloads FTXUI, so it needs internet once.

To try potter without touching your own tasks, history and settings, point it at empty folders:

```sh
XDG_CONFIG_HOME=/tmp/potter-test/config XDG_DATA_HOME=/tmp/potter-test/data ./build/potter
```

## How the code is organized

The [Project layout](README.md#project-layout) in the README lists every file. The short version:

- `src/core/` is pure logic: no UI and no system calls, so it can be unit-tested. New logic goes here if it can.
- `src/system/` talks to the OS and other programs (mpv, yt-dlp, notify-send).
- `src/ui/` draws the screens with FTXUI.
- `src/app.cpp` connects them and handles the keys.

## Tests

Each module in `src/core/` has its own test program in `tests/` (e.g. `tests/tasks_test.cpp`). They use a small `CHECK` macro instead of a test framework, so they build with nothing extra.

- If you change something in `src/core/`, add or update a test for it.
- A new test file needs three lines in `CMakeLists.txt`, like the existing `add_executable` / `target_link_libraries` / `add_test` blocks.

## Code style

There is no formatter config yet, so please match the code around your change:

- C++20, 2-space indentation, lines up to about 100 characters
- `#pragma once` in headers, and includes written from `src/` (`#include "core/timer.hpp"`)
- RAII: no raw `new` / `delete`, and `std::chrono` for all time math
- private members end in an underscore (`status_`)
- comments explain *why*, in plain English
- the build uses `-Wall -Wextra -Wpedantic`; please keep it free of warnings

## Pull requests

- Keep each PR to one change. A bug fix and a new feature are easier to review as two PRs.
- Describe what changed and how you tested it. A before/after list or table is great.
- Commit messages are short and say what changed, e.g. `fix: save state and clean up on SIGHUP`.
- CI builds and tests every PR on Linux (GCC and Clang), macOS and the static musl build. It needs to be green before merging. For a first PR, GitHub waits for the maintainer to start CI, so it may take a moment.
- You don't need to edit `CHANGELOG.md` or the version number; that happens at release time (see [RELEASING.md](RELEASING.md)).

## License

potter is licensed under the [GNU General Public License v3.0 or later](LICENSE). By contributing, you agree that your contribution is licensed under the same terms.

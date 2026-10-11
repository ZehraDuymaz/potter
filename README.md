# potter 🍅

A lightweight Pomodoro timer for the terminal.

potter takes the good parts of browser Pomodoro apps (focus cycles, tasks, streaks, ambient music) and puts them in a single terminal app that uses a few MB of RAM instead of a whole browser tab.

> Status: **v0.1** — timer + settings work. Tasks, streaks and music are next.

---

## Goals

- **Light.** A single small binary that idles near 0% CPU and uses under 10 MB of RAM.
- **Fits into a riced desktop.** The background stays transparent, so your terminal's opacity and blur show through. Colors come from the terminal palette by default, so pywal or matugen themes (Noctalia, for example) apply automatically.
- **Keyboard-first.** Every action has a key, and the mouse is optional.
- **Local only.** No accounts and no network, except for music you choose to play.
- **Accurate time.** The countdown is based on end times measured with `CLOCK_BOOTTIME`, not on counting ticks, so it doesn't drift and keeps counting while the laptop sleeps.

## Features

### v0.1: Timer and settings ✅

- Focus, short break and long break phases. The defaults are 25, 5 and 15 minutes, with a long break after every 4 focus sessions.
- A big centered countdown, a phase label, a session counter (e.g. `● ● ○ ○`) and a progress bar.
- Start, pause, reset and skip-phase controls.
- A settings screen inside the app for:
  - focus, short break and long break lengths
  - how many sessions come before a long break
  - auto-starting breaks and focus sessions
  - desktop notifications (`notify-send`) and a terminal bell when a phase ends
  - the color scheme: terminal palette (default) or custom hex colors
- Settings are saved to `~/.config/potter/config.toml`, and you can edit that file by hand too.
- The background is transparent: potter never paints one.

### v0.2: Tasks

- A to-do list beside the timer that you can add to, edit, complete, reorder and delete.
- Marking one task as the **active** task, so focus time is credited to it.
- Per-task time tracking, with subtasks that roll up into their parent.

### v0.3: Streaks and goals

- A daily focus goal (e.g. 4 sessions).
- A daily streak counter and a simple history view (sessions per day).

### v0.4: Music

- Ambient or lo-fi playback through `mpv` + `yt-dlp`, which accepts YouTube URLs or local files.
- Play, pause and volume controls inside potter, plus an option to pause the music during breaks.
- A saved list of favorite streams.

### Later ideas

- Waybar or Noctalia bar integration, via a `potter status` command that prints the remaining time as JSON.
- Control from scripts or keybinds (`potter toggle`, `potter skip`) through a small local socket.
- A compact mode for a tiny floating terminal window (Hyprland float rule), which works like picture-in-picture.
- Packaging for the AUR.

## Keybindings

| Key | Action |
| --- | --- |
| `space` | Start / pause |
| `r` | Reset current phase |
| `n` | Skip to next phase |
| `s` | Open settings |
| `t` | Toggle task panel (v0.2) |
| `m` | Toggle music (v0.4) |
| `?` | Help |
| `q` | Quit |

In settings: `↑↓`/`jk` move, `←→`/`hl` change (`H`/`L` ±5), `enter` toggle or edit a color, `esc` save and close.

## Tech choices

- **Language: C++20**, with [FTXUI](https://github.com/ArthurSonzogni/FTXUI) for the UI (components, layouts, mouse support).
  - CMake downloads FTXUI automatically at build time (`FetchContent`), so nothing needs to be installed by hand.
- **Build:** CMake (3.20+) with GCC or Clang.
- **Config:** TOML at `~/.config/potter/config.toml`, read with [toml++](https://github.com/marzer/tomlplusplus) (in the Arch `extra` repo, or fetched by CMake).
- **Data:** tasks, history and streaks in `~/.local/share/potter/` (following XDG).
- **Notifications:** `notify-send`. **Music:** `mpv` + `yt-dlp`. Both are optional runtime dependencies.
- **Style:** modern C++ with RAII, no raw `new`/`delete`, and `std::chrono` for all time math.

## Project layout

```
potter/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp           # start-up, screen + main loop
│   ├── app.hpp/.cpp       # app state + key handling
│   ├── timer.hpp/.cpp     # phase logic (pure, unit-tested)
│   ├── clock.hpp          # CLOCK_BOOTTIME clock (counts during sleep)
│   ├── config.hpp/.cpp    # load/save config.toml
│   ├── notify.hpp/.cpp    # notify-send
│   └── ui/
│       ├── views.hpp
│       ├── timer_view.cpp     # timer + help screens
│       └── settings_view.cpp  # settings screen
├── tests/
│   └── timer_test.cpp
└── docs/
```

`timer.cpp` is kept free of UI code, so the Pomodoro logic can be tested on its own.

## Requirements

- Linux
- A C++20 compiler (GCC 11+ or Clang 14+) and CMake 3.20+
- Internet access for the first build (CMake downloads FTXUI)
- Optional: `notify-send` (libnotify) for desktop notifications

On Arch / CachyOS:

```sh
sudo pacman -S --needed base-devel cmake libnotify tomlplusplus
```

On Debian / Ubuntu:

```sh
sudo apt install build-essential cmake libnotify-bin
```

## Build and run

```sh
git clone https://github.com/ZehraDuymaz/potter.git
cd potter
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/potter

# run the tests
ctest --test-dir build

# install to ~/.local/bin
cmake --install build --prefix ~/.local
```

The first configure downloads FTXUI (and toml++ if it isn't installed), so it needs internet once.

## License

potter is free software, licensed under the [GNU General Public License v3.0 or later](LICENSE).

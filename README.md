# potter 🤓

A lightweight Pomodoro timer for the terminal.

potter takes the good parts of browser Pomodoro apps (focus cycles, tasks, streaks, ambient music) and puts them in a single terminal app that uses a few MB of RAM instead of a whole browser tab.

> Status: **v0.3** — timer, settings, tasks, daily goal and streaks work. Music is next.

---

## Why Potter 
I use CachyOS + Hyprland + Noctalia Setup and I have a nice ytplay command where I can basically search in YouTube and play things using yt-dlp and mpv. And I think it is one of the most amazing things in my computer. Definitely not exaggerating. Moreover, I split my terminal and open cava and it is an amazing pleasure. And I also love using blanket and termdown and also a nice todos thingy. So, I thought what if we can combine these all to a single terminal app. 

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

### v0.2: Tasks ✅

- A to-do list beside the timer that you can add to, rename, complete, reorder and delete.
- One level of subtasks.
- Marking one task as the **active** task (`▶`), so focus time is credited to it as it happens. Pauses, resets, skips and breaks don't count.
- Per-task time tracking, with subtasks' time rolled up into their parent.
- Tasks are saved to `~/.local/share/potter/tasks.toml`, at least every 30 seconds while the timer runs and whenever you change something.

### v0.3: Streaks and goals ✅

- A daily focus goal in minutes (default 1h 40m, i.e. 4 × 25 min). Every focused minute counts, with or without an active task.
- Today's progress and your streak under the timer: `45m / 1h 40m today · 🔥 5`, plus a notification when you reach the goal.
- A streak counts consecutive days that met their goal. Today doesn't break it while it's still in progress.
- A new day starts at 4 AM by default (adjustable), so late-night focus counts for the day before.
- A history view (`h`): a vertical bar chart of the last 14 days with the goal line, current and best streak, and totals.
- History is saved to `~/.local/share/potter/history.toml`.

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
| `t` | Show / hide the task panel |
| `h` | History + streaks |
| `m` | Toggle music (v0.4) |
| `?` | Help |
| `q` | Quit |

In the task panel: `↑↓`/`jk` move, `a` add, `A` add subtask, `e` rename, `x` done, `enter` make active, `dd` delete, `J`/`K` move down/up.

In settings: `↑↓`/`jk` move, `←→`/`hl` change (`H`/`L` ±5), `enter` toggle or edit a color, `esc` save and close.

## Tech choices

- **Language: C++20**, with [FTXUI](https://github.com/ArthurSonzogni/FTXUI) for the UI (components, layouts, mouse support).
  - CMake downloads FTXUI automatically at build time (`FetchContent`), so nothing needs to be installed by hand.
- **Build:** CMake (3.20+) with GCC or Clang.
- **Config:** TOML at `~/.config/potter/config.toml`, read with [toml++](https://github.com/marzer/tomlplusplus) (in the Arch `extra` repo, or fetched by CMake).
- **Data:** tasks and focus history in `~/.local/share/potter/` (following XDG).
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
│   ├── timer.hpp/.cpp     # phase logic + focus meter (pure, unit-tested)
│   ├── tasks.hpp/.cpp     # task list + tasks.toml (pure, unit-tested)
│   ├── history.hpp/.cpp   # focus per day, goal, streaks (pure, unit-tested)
│   ├── clock.hpp          # CLOCK_BOOTTIME clock (counts during sleep)
│   ├── config.hpp/.cpp    # load/save config.toml
│   ├── storage.hpp/.cpp   # XDG paths + safe file writes
│   ├── notify.hpp/.cpp    # notify-send
│   └── ui/
│       ├── views.hpp
│       ├── timer_view.cpp     # timer + help screens
│       ├── task_view.cpp      # task panel
│       ├── history_view.cpp   # history bar chart
│       └── settings_view.cpp  # settings screen
├── tests/
│   ├── timer_test.cpp
│   ├── tasks_test.cpp
│   └── history_test.cpp
└── docs/
```

`timer.cpp`, `tasks.cpp` and `history.cpp` are kept free of UI code, so the logic can be tested on its own.

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

## Install from a release

Download the latest `potter-vX.Y.Z-linux-x86_64.tar.gz` from the [Releases page](https://github.com/ZehraDuymaz/potter/releases), then:

```sh
tar -xzf potter-*-linux-x86_64.tar.gz
install -Dm755 potter-*-linux-x86_64/potter ~/.local/bin/potter
potter --version
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

AI is used in this project. 

## License

potter is free software, licensed under the [GNU General Public License v3.0 or later](LICENSE).

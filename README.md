# potter 🤓

A lightweight Pomodoro timer for the terminal.

potter takes the good parts of browser Pomodoro apps (focus cycles, tasks, streaks, ambient music) and puts them in a single terminal app that's much lighter than a browser tab.

> Status: **v0.5** — timer, settings, tasks, daily goal, streaks, music and per-task history work.

---

<img width="1080" height="608" alt="pdemo" src="https://github.com/user-attachments/assets/4ab57e1b-470b-4d72-ae1f-28b340d274c1" />

---

## Why Potter 
I use CachyOS + Hyprland + Noctalia Setup and I have a nice ytplay command where I can basically search in YouTube and play things using yt-dlp and mpv. And I think it is one of the most amazing things in my computer. Definitely not exaggerating. Moreover, I split my terminal and open cava and it is an amazing pleasure. And I also love using blanket and termdown and also a nice todos thingy. So, I thought what if we can combine these all to a single terminal app. 

## Goals

- **Light.** potter is a small native program that barely touches your CPU and uses only a few MB of memory. Music plays through mpv, which needs more while it's playing, but the whole thing still stays far lighter than a browser tab.
- **Fits into a riced environment.** The background stays transparent, so your terminal's opacity and blur show through. Colors come from the terminal palette by default, so pywal or matugen themes (Noctalia, for example) apply automatically.
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

### v0.4: Music ✅

- A music screen (`m`) with one box for everything: type words to **search YouTube** (top 5 results, pick one with enter), paste a **link** to play it, or type a **file or folder path** (`~/Music/...`) to play local music.
- Playback runs through `mpv` in the background (audio only), controlled by potter. YouTube needs `yt-dlp`.
- Now playing under the timer: `♪ title · ▶ 1:23`.
- Play/pause with `p` and volume with `+`/`-` from anywhere; the volume is remembered.
- **Favorites:** press `f` while something plays to save it; replay it from the music screen. Saved to `~/.local/share/potter/favorites.toml`.
- Music pauses during breaks and resumes when focus starts (a setting, on by default). Pausing or playing by hand always wins.
- Music stops when you quit potter.

### v0.5: Task history ✅

- Your history now remembers **which task** your focus went to — even after you finish, rename or delete the task.
- In the history screen (`h`), `←`/`→` picks a day in the chart and shows where that day's focus went, task by task (plus "no task" time).
- A second tab (`tab`) shows focus totals per task for the last 7 days, 30 days or all time (`r` to switch), with each task's status: active, open, done or deleted.
- Days recorded before v0.5 keep their totals but have no per-task details.
- Since v0.5.1: select a task in the totals tab and press `dd` to remove its time from your history — handy for test runs. Its time also comes off those days' totals.

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
| `h` | History: days, task totals, streaks |
| `m` | Music screen |
| `p` | Play / pause music |
| `+` / `-` | Music volume |
| `?` | Help |
| `q` | Quit |

In the task panel: `↑↓`/`jk` move, `a` add, `A` add subtask, `e` rename, `x` done, `enter` make active, `dd` delete, `J`/`K` move down/up.

In the history screen: `←`/`→` pick a day, `tab` switch between days and task totals, `r` change the totals' range, `j`/`k` move, `dd` remove a task's time from the history, `esc` close.

In the music screen: `/` search or paste a link/path, `↑↓`/`jk` move, `enter` play, `p` pause, `+`/`-` volume, `f` add/remove favorite, `x` stop, `dd` remove a favorite, `esc` close.

In settings: `↑↓`/`jk` move, `←→`/`hl` change (`H`/`L` ±5), `enter` toggle or edit a color, `esc` save and close.

## Tech choices

- **Language: C++20**, with [FTXUI](https://github.com/ArthurSonzogni/FTXUI) for the UI (components, layouts, mouse support).
  - CMake downloads FTXUI automatically at build time (`FetchContent`), so nothing needs to be installed by hand.
- **Build:** CMake (3.20+) with GCC or Clang.
- **Config:** TOML at `~/.config/potter/config.toml`, read with [toml++](https://github.com/marzer/tomlplusplus) (in the Arch `extra` repo, or fetched by CMake).
- **Data:** tasks, focus history and favorites in `~/.local/share/potter/` (following XDG).
- **Notifications:** `notify-send`. **Music:** `mpv` (controlled over its JSON IPC socket, parsed with [nlohmann/json](https://github.com/nlohmann/json)) + `yt-dlp`. All optional at runtime: potter tells you what to install.
- **Style:** modern C++ with RAII, no raw `new`/`delete`, and `std::chrono` for all time math.

## Project layout

```
potter/
├── CMakeLists.txt
├── README.md
├── CHANGELOG.md
├── src/
│   ├── main.cpp               # start-up, screen + main loop
│   ├── app.hpp/.cpp           # glue: app state, key handling, connects everything
│   ├── core/                  # pure logic, no UI, unit-tested
│   │   ├── timer.hpp/.cpp     # Pomodoro phases + focus meter
│   │   ├── clock.hpp          # a clock that keeps counting during sleep
│   │   ├── tasks.hpp/.cpp     # task list + tasks.toml
│   │   ├── history.hpp/.cpp   # focus per day, daily goal, streaks
│   │   ├── music.hpp/.cpp     # input types, yt-dlp + mpv parsing, favorites
│   │   ├── config.hpp/.cpp    # load/save config.toml
│   │   └── storage.hpp/.cpp   # XDG paths + safe file writes
│   ├── system/                # talks to the OS and other programs
│   │   ├── process.hpp/.cpp   # starting programs without a shell
│   │   ├── notify.hpp/.cpp    # desktop notifications (notify-send)
│   │   ├── player.hpp/.cpp    # mpv in the background, over its IPC socket
│   │   └── search.hpp/.cpp    # YouTube search with yt-dlp, in the background
│   └── ui/                    # everything drawn on screen (FTXUI)
│       ├── views.hpp
│       ├── timer_view.cpp     # timer + help screens
│       ├── task_view.cpp      # task panel
│       ├── history_view.cpp   # history bar chart
│       ├── music_view.cpp     # music screen
│       └── settings_view.cpp  # settings screen
├── tests/                     # one test program per core module
│   ├── timer_test.cpp
│   ├── tasks_test.cpp
│   ├── history_test.cpp
│   └── music_test.cpp
├── packaging/
│   └── build-static.sh        # fully static Linux build (Alpine/musl, run via Docker)
└── docs/
```

Everything in `core/` is free of UI and system code, so the logic can be tested on its own. Includes are written from `src/`, e.g. `#include "core/timer.hpp"`.

## Requirements

- Linux
- A C++20 compiler (GCC 11+ or Clang 14+) and CMake 3.20+
- Internet access for the first build (CMake downloads FTXUI, and toml++ / nlohmann-json if they aren't installed)
- Optional: `notify-send` (libnotify) for desktop notifications
- Optional: `mpv` for music, plus `yt-dlp` for YouTube

On Arch / CachyOS:

```sh
sudo pacman -S --needed base-devel cmake libnotify tomlplusplus nlohmann-json mpv yt-dlp
```

On Debian / Ubuntu:

```sh
sudo apt install build-essential cmake libnotify-bin mpv yt-dlp
```

## Install from a release

The release binaries are **fully static**: one file that runs on any 64-bit Linux distro — Arch, Debian, Ubuntu, Fedora, openSUSE, old or new — for both **x86_64** and **ARM64** (e.g. Raspberry Pi), with nothing else to install.

```sh
# downloads the newest release for your CPU (x86_64 or aarch64)
curl -L "https://github.com/ZehraDuymaz/potter/releases/latest/download/potter-linux-$(uname -m).tar.gz" | tar -xz
install -Dm755 "potter-linux-$(uname -m)/potter" ~/.local/bin/potter
potter --version
```

Make sure `~/.local/bin` is on your `PATH`. To update, run the same commands again. Music still needs `mpv` (and `yt-dlp` for YouTube) from your distro's packages.

You can also download the files by hand from the [Releases page](https://github.com/ZehraDuymaz/potter/releases); each one comes with a `.sha256` checksum.

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

Built with the help of Claude Code. I designed the features, tested and released it, and I'm learning C++ through it.

## Troubleshooting

### Music doesn't play (or plays silently)

potter plays music through **your own mpv**, so your mpv config (`~/.config/mpv/mpv.conf` and `~/.config/mpv/scripts/`) also applies inside potter. potter overrides only what it needs (background mode, no video, its control socket, audio-only YouTube and the volume) — everything else in your config still counts.

**1. Check whether it's your config.** Try both commands in a normal terminal:

```sh
mpv --no-video "https://www.youtube.com/watch?v=JCKBaJDRMw4"               # with your config
mpv --no-config --no-video "https://www.youtube.com/watch?v=JCKBaJDRMw4"   # without it
```

If the second one plays and the first doesn't, something in your mpv config is the cause.

**2. Common culprits in `mpv.conf`:**

| Setting | What happens in potter |
| --- | --- |
| `mute=yes` | everything plays, but silently |
| `ytdl=no` | YouTube links and search results don't play |
| `script-opts=ytdl_hook-ytdl_path=youtube-dl` | YouTube fails if it points to the old `youtube-dl` — point it to `yt-dlp` or remove it |
| `audio-device=...` | no sound if that device isn't connected |
| scripts in `~/.config/mpv/scripts/` | some scripts expect a video window or a terminal and can get in the way |

**3. Or let potter ignore your mpv config.** Add this to `~/.config/potter/config.toml` — your mpv config stays untouched for everything else:

```toml
[music]
mpv_args = "--no-config"
```

`mpv_args` takes any mpv options, e.g. `"--no-config --audio-device=auto"`. Run `mpv --audio-device=help` to list your devices.

### YouTube search shows "search failed"

YouTube changes often, and an old `yt-dlp` stops working. Update it (`sudo pacman -Syu yt-dlp`, `brew upgrade yt-dlp`, or `yt-dlp -U` if you installed it yourself).

## License

potter is free software, licensed under the [GNU General Public License v3.0 or later](LICENSE).

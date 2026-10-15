# Changelog

All notable changes to potter are listed here.
Versions follow [Semantic Versioning](https://semver.org): `MAJOR.MINOR.PATCH`.

## [Unreleased]

## [0.5.3] - 2026-10-15

### Fixed
- Music failing with "couldn't play this: no audio or video data played" when your mpv config turns audio off (e.g. `no-audio` for a video wallpaper).

### Changed
- potter's mpv no longer reads `~/.config/mpv`; it uses `~/.config/potter/mpv` instead. To keep using your mpv settings in potter, put `include=~/.config/mpv/mpv.conf` in `~/.config/potter/mpv/mpv.conf`. `mpv_args` in config.toml works as before.
- README: the music troubleshooting section now describes potter's own mpv config folder.

## [0.5.2] - 2026-10-15

### Changed
- Release binaries are now fully static (built with musl in Alpine Linux), so they run on any 64-bit Linux distro, old or new — before, they needed a recent glibc.
- Releases now include an ARM64 (aarch64) build next to x86_64.
- Release files have fixed names (`potter-linux-x86_64.tar.gz`, `potter-linux-aarch64.tar.gz`), so `releases/latest/download/...` always points at the newest version; the README has a one-line install.
- CI also builds the static binary on x86_64 and ARM64 on every push.

### Added
- `packaging/build-static.sh`: the static build, runnable locally with Docker.
- README: troubleshooting for mpv configs and outdated yt-dlp.

## [0.5.1] - 2026-10-14

### Added
- Remove a task's time from the history: select it in the history's Tasks tab and press `dd` (asks first). Its time also comes off those days' totals, and days left empty are dropped — handy for cleaning up test runs.

### Changed
- The selected row in the history's Tasks tab is now highlighted.

## [0.5.0] - 2026-10-14

### Added
- History remembers which task each focus minute went to, with the task's title, so time stays visible after a task is finished, renamed or deleted. Focus without an active task is listed as "no task".
- History screen: `←`/`→` selects a day in the chart and shows its per-task breakdown.
- History screen: a "Tasks" tab (`tab`) with focus totals per task for the last 7 days, 30 days or all time (`r`).

### Changed
- The history screen closes with `esc` or `q` (other keys now navigate it).
- History screen layout matches the music screen (wider, padded).

## [0.4.0] - 2026-10-13

### Added
- Music screen (`m`): search YouTube (top results, pick one), paste a link, or play a local file/folder — all from one box.
- Playback through mpv in the background, controlled over its IPC socket; YouTube via yt-dlp.
- Now-playing line under the timer; `p` play/pause and `+`/`-` volume from anywhere (volume is remembered).
- Favorites (`f`), saved to `~/.local/share/potter/favorites.toml`.
- "Pause music during breaks" setting (on by default); music resumes when focus starts.
- Clear instructions when mpv or yt-dlp isn't installed.
- New dependency: nlohmann/json (system package, or downloaded by CMake).

### Changed
- Music stops when potter quits; Ctrl-C quits cleanly as before.
- Key hints under the timer now mention music; "s settings" moved to the first line.
- Source code reorganized into `src/core` (pure logic), `src/system` (OS and other programs) and `src/ui`.

## [0.3.0] - 2026-10-13

### Added
- Daily focus goal in minutes (default 1h 40m), set in the settings screen.
- Today's progress and streak under the timer, and a notification when the goal is reached.
- Streaks: consecutive days that met their goal; today doesn't break a streak while in progress.
- "New day starts at" setting (default 04:00), so late-night focus counts for the day before.
- History view (`h`): vertical bar chart of the last 14 days with goal line, current/best streak and totals.
- Focus history saved to `~/.local/share/potter/history.toml`.

### Changed
- The "N today" session count under the timer is replaced by today's focus time toward the goal.

## [0.2.1] - 2026-10-12

### Fixed
- The typing cursor jumped up to the task list when the text field was empty.
- The typing cursor appeared at the far end of an empty text field instead of the start.
- Long task titles were cut off; they now wrap onto more lines (also under the timer).

## [0.2.0] - 2026-10-12

### Added
- Task panel next to the timer (`t` to show/hide): add, rename, complete, reorder and delete tasks.
- One level of subtasks.
- Active task (`enter`): focus time is credited to it as it happens; subtask time rolls up into the parent.
- Tasks saved to `~/.local/share/potter/tasks.toml`.

### Changed
- Ctrl-C now quits cleanly and saves, like `q`.
- The key hints under the timer are split over two lines.

## [0.1.0] - 2026-10-11

### Added
- `potter --version` and `potter --help`.
- GitHub Actions: CI builds and tests every push; tagged versions are released automatically.
- Pomodoro timer with focus, short break and long break phases.
- Big countdown, progress bar and session dots.
- In-app settings screen, saved to `~/.config/potter/config.toml`.
- Desktop notifications (`notify-send`) and terminal bell.
- Terminal-palette or custom hex colors; transparent background.

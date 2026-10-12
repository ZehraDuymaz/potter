# Changelog

All notable changes to potter are listed here.
Versions follow [Semantic Versioning](https://semver.org): `MAJOR.MINOR.PATCH`.

## [Unreleased]

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

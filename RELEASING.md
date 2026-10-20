# Releasing potter

A checklist for a new version, e.g. `0.6.0`. Commands run from the repo root.

## 1. Prepare

- Bump the version in `CMakeLists.txt` (`project(potter VERSION 0.5.4 ...)`). It's the only place the version lives in the code.
- In `CHANGELOG.md`, rename `[Unreleased]` to `[0.5.4] - YYYY-MM-DD` and add a new empty `[Unreleased]` above it.
- Build and test: `cmake --build build && ctest --test-dir build`.

## 2. Release on GitHub

```sh
git add . && git commit -m "v0.6.0: ..." && git push
# wait until every CI job is green (Linux, macOS, static x86_64 + ARM64)
git tag -a v0.6.0 -m "potter 0.6.0"
git push origin v0.6.0
```

The tag starts `.github/workflows/release.yml`: it builds the static Linux binaries and publishes the release (a few minutes). If the tag doesn't match `CMakeLists.txt`, it stops with an error.

## 3. Update the packages

Once the release page shows the files:

```sh
packaging/update-packages.sh 0.6.0 ../homebrew-tap
```

It sets the new version and checksums in `packaging/aur/*/PKGBUILD` (+ `.SRCINFO`) and in the Homebrew formula. Then:

```sh
# this repo
git add packaging && git commit -m "packaging: 0.6.0" && git push

# Homebrew tap: its CI installs and tests the formula on macOS and Linux
cd ../homebrew-tap && git add . && git commit -m "potter 0.6.0" && git push
```

### AUR

Each AUR package is its own small git repo. Copy the `PKGBUILD` and `.SRCINFO` in and push:

```sh
cp packaging/aur/potter/PKGBUILD packaging/aur/potter/.SRCINFO ../aur-potter/
cd ../aur-potter && git add PKGBUILD .SRCINFO && git commit -m "0.6.0" && git push
# the same for potter-bin in ../aur-potter-bin
```

First time only (after creating an AUR account and adding your SSH key to it):

```sh
git clone ssh://aur@aur.archlinux.org/potter.git ../aur-potter
git clone ssh://aur@aur.archlinux.org/potter-bin.git ../aur-potter-bin
```

Cloning a name that doesn't exist yet gives an empty repo; the first push creates the package. Before pushing, a quick local check: `cd packaging/aur/potter && makepkg -si` (builds, runs the tests and installs it). Afterwards, update the "Arch Linux (AUR)" section of the README.

## If something needs fixing

- **Only the package recipe changed** (same potter version): raise `pkgrel` in the PKGBUILD (`1` → `2`) and run `makepkg --printsrcinfo > .SRCINFO`. For Homebrew, add `revision 1` under `license`.
- **FTXUI version changed** in `CMakeLists.txt`: update `_ftxuiver` in `packaging/aur/potter/PKGBUILD` (then `updpkgsums`) and the `resource "ftxui"` url + sha256 in the formula (`sha256sum` of the downloaded tarball).
- **The release stopped with "Tag … does not match CMakeLists.txt version":** nothing was published, so the tag can be redone. Fix the version (and the CHANGELOG), commit and push, then delete the tag and tag again:
  ```sh
  git tag -d v0.6.0
  git push origin :refs/tags/v0.6.0
  git tag -a v0.6.0 -m "potter 0.6.0"
  git push origin v0.6.0
  ```
- **Never move a published tag** (one whose release is out). Fix it in a new version instead.

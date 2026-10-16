#!/bin/sh
# After a release is published on GitHub, points the AUR packages and the
# Homebrew formula at it (version + checksums). Run on Arch from the repo root:
#
#   packaging/update-packages.sh 0.5.4 ../homebrew-tap
#
# Then commit/push this repo, the AUR repos and the tap (see RELEASING.md).
set -eu

version="${1:?usage: packaging/update-packages.sh VERSION [HOMEBREW_TAP_DIR]}"
tap="${2:-../homebrew-tap}"
url="https://github.com/ZehraDuymaz/potter"

# Fail early if the release isn't out yet.
curl -fsIL "$url/releases/download/v$version/potter-linux-x86_64.tar.gz" >/dev/null ||
  { echo "error: release v$version isn't published yet" >&2; exit 1; }

# AUR: new version, release 1, fresh checksums, fresh .SRCINFO.
for pkg in potter potter-bin; do
  (
    cd "packaging/aur/$pkg"
    sed -i -e "s/^pkgver=.*/pkgver=$version/" -e "s/^pkgrel=.*/pkgrel=1/" PKGBUILD
    updpkgsums
    makepkg --printsrcinfo > .SRCINFO
    rm -f ./*.tar.gz
  )
  echo "updated packaging/aur/$pkg"
done

# Homebrew: the source tarball's URL and its checksum (the first sha256 line;
# the second one belongs to FTXUI and stays).
formula="$tap/Formula/potter.rb"
if [ -f "$formula" ]; then
  sha=$(curl -fsL "$url/archive/refs/tags/v$version.tar.gz" | sha256sum | cut -d' ' -f1)
  sed -i -E \
    -e "s#(potter/archive/refs/tags/v)[0-9.]+(\.tar\.gz)#\1$version\2#" \
    -e "0,/sha256 \"[0-9a-f]+\"/s//sha256 \"$sha\"/" \
    "$formula"
  echo "updated $formula"
else
  echo "skipped Homebrew: $formula not found"
fi

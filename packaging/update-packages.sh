#!/bin/sh
# After a release is published on GitHub, points the AUR packages and the
# Homebrew formula at it (version + checksums). Run on Arch from the repo root:
#
#   packaging/update-packages.sh 0.7.1 ../homebrew-tap
#
# Leave out the tap folder to update only the AUR files.
# Then commit/push this repo, the AUR repos and the tap (see RELEASING.md).
set -eu

version="${1:?usage: packaging/update-packages.sh VERSION [HOMEBREW_TAP_DIR]}"
tap="${2:-}"
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

# Homebrew, when the tap folder is given.
if [ -n "$tap" ]; then
  "$(dirname "$0")/update-formula.sh" "$version" "$tap/Formula/potter.rb"
else
  echo "skipped Homebrew: no tap folder given"
fi

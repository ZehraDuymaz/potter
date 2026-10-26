#!/bin/sh
# Points the Homebrew formula at a published release: the source tarball's
# URL and its checksum. Used by update-packages.sh and by the release workflow.
#
#   packaging/update-formula.sh 0.7.1 ../homebrew-tap/Formula/potter.rb
set -eu

version="${1:?usage: packaging/update-formula.sh VERSION FORMULA}"
formula="${2:?usage: packaging/update-formula.sh VERSION FORMULA}"
url="https://github.com/ZehraDuymaz/potter/archive/refs/tags/v$version.tar.gz"

sha=$(curl -fsSL "$url" | sha256sum | cut -d' ' -f1)
# The first sha256 line is potter's; the later ones belong to FTXUI and
# toml++ and stay as they are.
sed -i -E \
  -e "s#(potter/archive/refs/tags/v)[0-9.]+(\.tar\.gz)#\1$version\2#" \
  -e "0,/sha256 \"[0-9a-f]+\"/s//sha256 \"$sha\"/" \
  "$formula"
echo "updated $formula to $version"

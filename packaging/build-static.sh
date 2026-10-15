#!/bin/sh
# Builds a fully static potter binary (musl libc), which runs on any 64-bit
# Linux distro, old or new, with nothing else to install.
#
# Run it inside an Alpine container from the repository root:
#
#   docker run --rm -v "$PWD":/src -w /src \
#     -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
#     alpine:3.21 sh packaging/build-static.sh
#
# The binary ends up in build-static/potter.
set -eu

apk add --no-cache build-base cmake linux-headers ca-certificates

cmake -S . -B build-static \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS="-static"
cmake --build build-static -j"$(nproc)"
ctest --test-dir build-static --output-on-failure

# Prove it really is static: a dynamic binary would list shared libraries.
if ldd build-static/potter >/dev/null 2>&1; then
  echo "error: build-static/potter is not statically linked" >&2
  exit 1
fi
# Drop debug symbols: users don't need them, and the download gets smaller.
strip build-static/potter
build-static/potter --version

# Files created in the container belong to root; hand them back.
if [ -n "${HOST_UID:-}" ]; then
  chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" build-static
fi

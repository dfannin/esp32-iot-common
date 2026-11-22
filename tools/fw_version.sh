#!/bin/sh
# fw_version.sh - write version.h for a device sketch from git.
#
# Firmware version convention (iot-platform architecture §6.4):
#   YYMMDD-HHMM         git committer time of HEAD, in UTC (a clean commit)
#   YYMMDD-HHMM-dirty   the same, with uncommitted changes to tracked files
#   ...-local           no git or no version.h: fallback in iot_version.h
# plus FW_BUILD, the short commit hash.
#
# Usage: fw_version.sh [sketch folder]    (default: the current folder)
# Run it before every compile (the device CI and the build command do).
# version.h is written only when it changes, and must be in .gitignore.

set -eu

cd "${1:-.}"
out=version.h

if ! git rev-parse -q --verify HEAD >/dev/null 2>&1; then
  echo "fw_version.sh: $(pwd) is not a git checkout; the build uses the -local fallback" >&2
  rm -f "$out"
  exit 0
fi

version=$(TZ=UTC0 git log -1 --date=format-local:%y%m%d-%H%M --format=%cd HEAD)
build=$(git rev-parse --short=7 HEAD)
# Tracked files only: ignored files (secrets.h, settings.h, version.h) and
# untracked files do not make a build dirty.
if ! git diff --quiet HEAD --; then
  version="$version-dirty"
fi

new=$(printf '%s\n' \
  "// Written by esp32-iot-common tools/fw_version.sh; not in git." \
  "#pragma once" \
  "#define FW_VERSION \"$version\"" \
  "#define FW_BUILD \"$build\"")

if [ ! -f "$out" ] || [ "$(cat "$out")" != "$new" ]; then
  printf '%s\n' "$new" > "$out"
fi
echo "firmware $version (build $build)"

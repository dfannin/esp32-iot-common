#!/bin/sh
# Test tools/fw_version.sh in a temporary git repository: sh tests/test_fw_version.sh
set -eu
script="$(cd "$(dirname "$0")/.." && pwd)/tools/fw_version.sh"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
check() { if [ "$1" = "$2" ]; then echo "ok   $3"; else echo "FAIL $3: got '$1', want '$2'"; fail=1; fi; }

cd "$tmp"
git init -q .
git config user.name test && git config user.email test@example.com
printf 'version.h\nsecrets.h\n' > .gitignore
echo "// sketch" > sketch.ino
git add . 
# Committed at 23:45 on 2026-11-21 in UTC-8, which is 07:45 on 2026-11-22 in UTC
GIT_COMMITTER_DATE="2026-11-21T23:45:10-08:00" GIT_AUTHOR_DATE="2026-01-01T00:00:00Z" git commit -q -m first
hash=$(git rev-parse --short=7 HEAD)

"$script" . >/dev/null
check "$(grep FW_VERSION version.h)" '#define FW_VERSION "261122-0745"' "clean commit: committer time in UTC"
check "$(grep FW_BUILD version.h)" "#define FW_BUILD \"$hash\"" "build is the short hash"

echo "secret" > secrets.h; echo "new" > untracked.txt
"$script" . >/dev/null
check "$(grep FW_VERSION version.h)" '#define FW_VERSION "261122-0745"' "ignored and untracked files are not dirty"

echo "// changed" >> sketch.ino
"$script" . >/dev/null
check "$(grep FW_VERSION version.h)" '#define FW_VERSION "261122-0745-dirty"' "uncommitted change: -dirty"

git checkout -q -- sketch.ino
"$script" . >/dev/null
check "$(grep FW_VERSION version.h)" '#define FW_VERSION "261122-0745"' "clean again"
touch -d "2000-01-01 00:00:00" version.h   # an old timestamp: unchanged content must keep it
"$script" . >/dev/null
check "$(date -r version.h +%Y)" "2000" "version.h rewritten only when it changes (no needless rebuild)"

mkdir "$tmp/nogit" && cd "$tmp/nogit" && touch version.h
GIT_CEILING_DIRECTORIES="$tmp" "$script" . 2>/dev/null >/dev/null
check "$([ -f version.h ] && echo present || echo absent)" "absent" "no git: version.h removed, -local fallback"

exit $fail

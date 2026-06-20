#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"

# run() en dry-run imprime el comando con prefijo y NO lo ejecuta
DRY_RUN=1
out="$(run touch "$here/__should_not_exist__")"
assert_contains "$out" "+ touch" "dry-run imprime comando"
[ -e "$here/__should_not_exist__" ] && { echo "FAIL: dry-run ejecutó el comando" >&2; rm -f "$here/__should_not_exist__"; }

# run() en modo real SÍ ejecuta
DRY_RUN=0
tmp="$(mktemp)"; rm -f "$tmp"
run touch "$tmp"
[ -f "$tmp" ] && assert_eq "ok" "ok" "modo real ejecuta" || assert_eq "ok" "no" "modo real ejecuta"
rm -f "$tmp"

finish

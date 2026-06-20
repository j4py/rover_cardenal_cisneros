#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
rc=0
for t in "$here"/test_*.sh; do
  [ -f "$t" ] || continue
  bash "$t" || rc=1
done
if [ "$rc" -eq 0 ]; then
  echo "TODAS LAS PRUEBAS OK"
else
  echo "HAY PRUEBAS FALLIDAS" >&2
fi
exit "$rc"

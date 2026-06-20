#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"

export HOST="rover.example.com" PORT="443"
out="$(render_template "$here/fixtures/sample.tmpl" '$HOST $PORT')"
assert_contains "$out" "host=rover.example.com" "sustituye HOST"
assert_contains "$out" "port=443" "sustituye PORT"
# Variables no listadas se dejan literales (no se expande $NO_TOCAR)
assert_contains "$out" 'literal=$NO_TOCAR' "no expande variables fuera de la lista"

finish

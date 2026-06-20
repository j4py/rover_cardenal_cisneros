#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/packages.sh"

DRY_RUN=1
out="$(apt_install git curl 2>&1)"
assert_contains "$out" "apt-get install" "apt_install planea instalar"
assert_contains "$out" "git" "incluye git"
assert_contains "$out" "curl" "incluye curl"
finish

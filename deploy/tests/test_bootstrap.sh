#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
# ROVER_DIR apunta a una ruta inexistente para forzar la rama de 'git clone'
out="$(ROVER_DIR=/tmp/rover-bootstrap-nonexistent bash "$here/../bootstrap.sh" --dry-run 2>&1)"
assert_contains "$out" "git clone" "clona el repo"
assert_contains "$out" "deploy.sh" "lanza el orquestador"
finish

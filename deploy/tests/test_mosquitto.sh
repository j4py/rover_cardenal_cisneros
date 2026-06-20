#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/mosquitto.sh"

# La plantilla rinde con los 2 listeners y auth
out="$(render_template "$here/../templates/mosquitto-rover.conf.tmpl")"
assert_contains "$out" "listener 1883" "listener nativo"
assert_contains "$out" "listener 9001" "listener websockets"
assert_contains "$out" "allow_anonymous false" "auth activada"

DRY_RUN=1
out="$(install_mosquitto "rover" "secreto" 2>&1)"
assert_contains "$out" "apt-get install" "instala mosquitto"
assert_contains "$out" "mosquitto_passwd" "crea usuario"
assert_contains "$out" "systemctl" "habilita servicio"
finish

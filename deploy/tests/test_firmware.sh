#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/firmware.sh"

work="$(mktemp -d)"
cp "$here/fixtures/sample.ino" "$work/sample.ino"

# Rellena: 1 red wifi, mqtt host = IP, video host = IP
fill_firmware_file "$work/sample.ino" \
  "MiWifi" "clave123" "" "" "" "" \
  "192.168.1.50" "rover" "secreto" "192.168.1.50"

body="$(cat "$work/sample.ino")"
assert_contains "$body" 'ssid_primary = "MiWifi"' "wifi primary"
assert_contains "$body" 'password_primary = "clave123"' "pass primary"
# Slots no usados copian el primario (WiFiMulti lo deduplica, inofensivo)
assert_contains "$body" 'ssid_secondary = "MiWifi"' "slot2 = primario"
assert_contains "$body" 'mqtt_server = "192.168.1.50"' "mqtt host"
assert_contains "$body" 'mqtt_password = "secreto"' "mqtt pass"
assert_contains "$body" 'ws_host = "192.168.1.50"' "video host"
assert_not_contains "$body" "CAMBIA_" "no quedan placeholders"

# reset restaura placeholders
bash "$here/../scripts/reset-firmware-secrets.sh" "$work/sample.ino"
body="$(cat "$work/sample.ino")"
assert_contains "$body" 'ssid_primary = "CAMBIA_WIFI_SSID"' "reset restaura placeholder"
assert_not_contains "$body" "MiWifi" "reset elimina secretos"

rm -rf "$work"
finish

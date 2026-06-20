#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/webgen.sh"

TPL_DIR="$here/../templates"
out="$(mktemp)"

# Modo LAN
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" lan "192.168.1.50" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "192.168.1.50"' "LAN host = IP"
assert_contains "$body" 'PORT: 80' "LAN port 80"
assert_contains "$body" 'USERNAME: "rover"' "LAN user"
assert_contains "$body" 'VIDEO_WS_URL = "ws://192.168.1.50:9002"' "LAN video ws"

# Modo DDNS
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" ddns "rover.duckdns.org" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "rover.duckdns.org"' "DDNS host"
assert_contains "$body" 'PORT: 443' "DDNS port 443"
assert_contains "$body" 'VIDEO_WS_URL = "wss://rover.duckdns.org/video"' "DDNS video wss"

# Modo Cloudflare
generate_mqtt_js "$TPL_DIR/mqtt.js.tmpl" "$out" cloudflare "example.com" "rover" "secreto"
body="$(cat "$out")"
assert_contains "$body" 'HOST: "mqtt.example.com"' "CF host = mqtt.<dominio>"
assert_contains "$body" 'PORT: 443' "CF port 443"
assert_contains "$body" 'VIDEO_WS_URL = "wss://video.example.com"' "CF video wss"

rm -f "$out"
finish

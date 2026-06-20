#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"

# Ejecuta el orquestador en dry-run + no interactivo con la fixture LAN.
out="$(bash "$here/../deploy.sh" --dry-run --env "$here/fixtures/answers-lan.env" --skip-clone 2>&1)"

assert_contains "$out" "núcleo" "informa del núcleo obligatorio"
assert_contains "$out" "mosquitto" "planifica mosquitto"
assert_contains "$out" "nginx" "planifica nginx"
assert_contains "$out" "video_proxy" "planifica video_proxy"
assert_not_contains "$out" "cloudflared service install" "LAN no instala cloudflared"
assert_not_contains "$out" "certbot" "LAN no usa certbot"
assert_contains "$out" "RESUMEN" "muestra resumen final"
finish

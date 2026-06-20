#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/cloudflare.sh"
. "$here/../lib/tls.sh"

DRY_RUN=1
out="$(install_cloudflared "TOKEN_DEMO" 2>&1)"
assert_contains "$out" "cloudflared" "instala cloudflared"
assert_contains "$out" "service install" "instala servicio con token"

out="$(install_tls "rover.duckdns.org" "yo@example.com" 2>&1)"
assert_contains "$out" "certbot" "usa certbot"
assert_contains "$out" "rover.duckdns.org" "incluye dominio"
finish

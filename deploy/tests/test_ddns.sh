#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/ddns.sh"

# Render por proveedor
export DDNS_DOMAIN="rover" DDNS_TOKEN="tok123"
out="$(render_template "$here/../templates/ddns-duckdns.sh.tmpl" '$DDNS_DOMAIN $DDNS_TOKEN')"
assert_contains "$out" "duckdns.org/update?domains=rover&token=tok123" "duckdns url"
out="$(render_template "$here/../templates/ddns-dynu.sh.tmpl" '$DDNS_DOMAIN $DDNS_TOKEN')"
assert_contains "$out" "api.dynu.com/nic/update?hostname=rover&password=tok123" "dynu url"

# La función selecciona la plantilla correcta del proveedor
assert_eq "ddns-duckdns.sh.tmpl" "$(ddns_template_for duckdns)" "tmpl duckdns"
assert_eq "ddns-ydns.sh.tmpl" "$(ddns_template_for ydns)" "tmpl ydns"

DRY_RUN=1
out="$(install_ddns duckdns "rover" "tok123" 2>&1)"
assert_contains "$out" "rover-ddns-update.sh" "instala updater"
assert_contains "$out" "ddns-updater.timer" "instala timer"
assert_contains "$out" "systemctl" "habilita timer"
finish

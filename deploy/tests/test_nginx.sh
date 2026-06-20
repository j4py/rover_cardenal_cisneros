#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/nginx.sh"

export SERVER_NAME="_" WEBROOT="/opt/rover"
out="$(render_template "$here/../templates/nginx-rover.conf.tmpl" '$SERVER_NAME $WEBROOT')"
assert_contains "$out" "root /opt/rover;" "root parametrizado"
assert_contains "$out" "location /mqtt" "proxy mqtt"
assert_contains "$out" "location /video" "proxy video"
assert_contains "$out" "location /publish" "proxy publish"
assert_contains "$out" 'try_files $uri' "literal nginx preservado"

DRY_RUN=1
out="$(install_nginx "_" "/opt/rover" 2>&1)"
assert_contains "$out" "apt-get install" "instala nginx"
assert_contains "$out" "sites-available/rover" "escribe site"
assert_contains "$out" "systemctl" "recarga servicio"
finish

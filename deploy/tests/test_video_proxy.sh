#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/video_proxy.sh"

export PROXY_PATH="/opt/rover-proxy/video_proxy.py" PROXY_DIR="/opt/rover-proxy" PROXY_USER="rover"
out="$(render_template "$here/../templates/video_proxy.service.tmpl" '$PROXY_PATH $PROXY_DIR $PROXY_USER')"
assert_contains "$out" "ExecStart=/usr/bin/python3 /opt/rover-proxy/video_proxy.py" "execstart"
assert_contains "$out" "User=rover" "user"

DRY_RUN=1
out="$(install_video_proxy "rover" 2>&1)"
assert_contains "$out" "python3" "instala python"
assert_contains "$out" "video_proxy.service" "instala servicio"
assert_contains "$out" "systemctl" "habilita servicio"
finish

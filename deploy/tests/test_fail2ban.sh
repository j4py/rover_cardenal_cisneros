#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/template.sh"
. "$here/../lib/packages.sh"
. "$here/../lib/fail2ban.sh"

DRY_RUN=1
out="$(install_fail2ban 2>&1)"
assert_contains "$out" "apt-get install" "instala fail2ban"
assert_contains "$out" "jail.d/sshd.local" "escribe jail"
assert_contains "$out" "systemctl" "habilita servicio"
finish

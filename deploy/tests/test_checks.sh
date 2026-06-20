#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/checks.sh"

is_ubuntu "$here/fixtures/os-release-ubuntu" && assert_eq ok ok "detecta ubuntu" || assert_eq ok no "detecta ubuntu"
is_ubuntu "$here/fixtures/os-release-debian" && assert_eq ok no "rechaza debian" || assert_eq ok ok "rechaza debian"

need_cmd bash && assert_eq ok ok "need_cmd encuentra bash" || assert_eq ok no "need_cmd encuentra bash"
need_cmd __comando_inexistente__ && assert_eq ok no "need_cmd falla con inexistente" || assert_eq ok ok "need_cmd falla con inexistente"

finish

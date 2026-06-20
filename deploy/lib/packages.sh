#!/usr/bin/env bash
# Instalación de paquetes vía apt. Requiere core.sh.

APT_UPDATED=0

apt_update_once() {
  [ "$APT_UPDATED" = "1" ] && return 0
  run_root apt-get update -y
  APT_UPDATED=1
}

# apt_install PKG...
apt_install() {
  [ "$#" -gt 0 ] || return 0
  apt_update_once
  run_root env DEBIAN_FRONTEND=noninteractive apt-get install -y "$@"
}

# install_base: dependencias mínimas siempre necesarias
install_base() {
  info "Se va a instalar (núcleo): git, curl, ca-certificates, gettext-base"
  apt_install git curl ca-certificates gettext-base
}

#!/usr/bin/env bash
# Comprobaciones de preflight.

# is_ubuntu [ruta-os-release]
is_ubuntu() {
  local f="${1:-/etc/os-release}"
  [ -f "$f" ] || return 1
  grep -q '^ID=ubuntu' "$f"
}

# need_cmd está definido en core.sh (helper compartido).

# Asegura que tenemos sudo (o somos root). Cachea el timestamp.
ensure_sudo() {
  [ "$(id -u)" -eq 0 ] && return 0
  need_cmd sudo || die "Se necesita 'sudo' o ejecutar como root."
  info "Algunas acciones requieren privilegios; se pedirá tu contraseña de sudo."
  if [ "$DRY_RUN" != "1" ]; then
    sudo -v || die "No se pudo obtener sudo."
  fi
}

# port_in_use PUERTO -> 0 si está ocupado
port_in_use() {
  local p="$1"
  if need_cmd ss; then
    ss -ltn 2>/dev/null | grep -Eq "[:.]$p[[:space:]]"
  else
    return 1
  fi
}

check_internet() {
  if need_cmd curl; then
    curl -fsS --max-time 8 -o /dev/null https://github.com 2>/dev/null
  else
    return 0
  fi
}

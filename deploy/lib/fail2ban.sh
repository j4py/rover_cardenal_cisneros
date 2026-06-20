#!/usr/bin/env bash
# Endurecimiento SSH con fail2ban (componente OPCIONAL).
DEPLOY_DIR="${DEPLOY_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

install_fail2ban() {
  info "Instalando fail2ban (jail sshd)"
  apt_install fail2ban
  local tmp; tmp="$(mktemp)"
  render_to "$DEPLOY_DIR/templates/fail2ban-sshd.local.tmpl" "" "$tmp"
  run_root install -m 0644 "$tmp" /etc/fail2ban/jail.d/sshd.local
  rm -f "$tmp"
  run_root systemctl enable --now fail2ban
  run_root systemctl restart fail2ban
  ok "fail2ban configurado"
}

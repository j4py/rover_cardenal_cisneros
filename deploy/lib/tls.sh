#!/usr/bin/env bash
# TLS con certbot para el hostname DDNS (modo DDNS).

# install_tls HOSTNAME EMAIL
install_tls() {
  local host="$1" email="$2"
  info "Solicitando certificado TLS para $host (certbot)"
  apt_install certbot python3-certbot-nginx
  run_root certbot --nginx -d "$host" --non-interactive --agree-tos -m "$email" --redirect
  ok "TLS configurado para $host"
}

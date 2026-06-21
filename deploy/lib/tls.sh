#!/usr/bin/env bash
# TLS con certbot para el hostname DDNS (modo DDNS).

# install_tls HOSTNAME EMAIL
install_tls() {
  local host="$1" email="$2"
  info "Solicitando certificado TLS para $host (certbot)"
  apt_install certbot python3-certbot-nginx
  run_root certbot --nginx -d "$host" --non-interactive --agree-tos -m "$email" --redirect
  
  info "Adaptando configuración de Nginx para usar el puerto 8443..."
  run_root sed -i 's/listen 443 ssl/listen 8443 ssl/g' /etc/nginx/sites-available/rover
  run_root sed -i 's/listen \[::\]:443 ssl/listen \[::\]:8443 ssl/g' /etc/nginx/sites-available/rover
  run_root sed -i 's/return 301 https:\/\/\$host\$request_uri/return 301 https:\/\/\$host:8443\$request_uri/g' /etc/nginx/sites-available/rover
  run_root systemctl reload nginx
  
  ok "TLS configurado para $host en el puerto 8443"
}

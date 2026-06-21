#!/usr/bin/env bash
# Rellena los tokens CAMBIA_* en los .ino con las credenciales del usuario.

# _sed_inplace EXPR FILE  (sed -i portable)
_sed_inplace() {
  local expr="$1" file="$2"
  sed -i.bak "$expr" "$file" && rm -f "$file.bak"
}

# _replace_token TOKEN VALUE FILE  (sustituye el token literal por VALUE, escapando / y &)
_replace_token() {
  local token="$1" value="$2" file="$3" esc
  esc=$(printf '%s' "$value" | sed -e 's/[\/&]/\\&/g')
  _sed_inplace "s/$token/$esc/g" "$file"
}

# fill_firmware_file FILE  S1 P1 S2 P2 S3 P3  MQTT_HOST MQTT_USER MQTT_PASS VIDEO_HOST VIDEO_PORT
fill_firmware_file() {
  local file="$1"; shift
  local s1="$1" p1="$2" s2="$3" p2="$4" s3="$5" p3="$6"
  local mhost="$7" muser="$8" mpass="$9" vhost="${10:-}" vport="${11:-}"
  [ -f "$file" ] || die "Firmware no encontrado: $file"
  # Slots vacíos copian el primario (inofensivo con WiFiMulti).
  [ -z "$s2" ] && s2="$s1"; [ -z "$p2" ] && p2="$p1"
  [ -z "$s3" ] && s3="$s1"; [ -z "$p3" ] && p3="$p1"
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ rellenar credenciales en %s\n' "$file"
    return 0
  fi
  _replace_token CAMBIA_WIFI_SSID2 "$s2" "$file"
  _replace_token CAMBIA_WIFI_PASS2 "$p2" "$file"
  _replace_token CAMBIA_WIFI_SSID3 "$s3" "$file"
  _replace_token CAMBIA_WIFI_PASS3 "$p3" "$file"
  _replace_token CAMBIA_WIFI_SSID  "$s1" "$file"
  _replace_token CAMBIA_WIFI_PASS  "$p1" "$file"
  _replace_token CAMBIA_MQTT_HOST  "$mhost" "$file"
  _replace_token CAMBIA_MQTT_USER  "$muser" "$file"
  _replace_token CAMBIA_MQTT_PASS  "$mpass" "$file"
  _replace_token CAMBIA_VIDEO_HOST "$vhost" "$file"
  _replace_token CAMBIA_VIDEO_PORT "$vport" "$file"
}

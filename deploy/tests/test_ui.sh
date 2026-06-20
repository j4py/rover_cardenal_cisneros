#!/usr/bin/env bash
set -u
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/lib.sh"
. "$here/../lib/core.sh"
. "$here/../lib/ui.sh"

# Validadores
validate_ip "192.168.1.10"   && assert_eq ok ok "ip válida" || assert_eq ok no "ip válida"
validate_ip "999.1.1.1"      && assert_eq ok no "ip inválida rechazada" || assert_eq ok ok "ip inválida rechazada"
validate_hostname "rover.example.com" && assert_eq ok ok "host válido" || assert_eq ok no "host válido"
validate_hostname "no validez!" && assert_eq ok no "host inválido rechazado" || assert_eq ok ok "host inválido rechazado"
validate_nonempty "x" && assert_eq ok ok "nonempty ok" || assert_eq ok no "nonempty ok"
validate_nonempty ""  && assert_eq ok no "nonempty vacío rechazado" || assert_eq ok ok "nonempty vacío rechazado"

# ask usa el default cuando la entrada está vacía
out="$(printf '\n' | ask "Nombre" "valor_por_defecto")"
assert_eq "valor_por_defecto" "$out" "ask devuelve default con entrada vacía"

# ask devuelve lo introducido
out="$(printf 'introducido\n' | ask "Nombre" "def")"
assert_eq "introducido" "$out" "ask devuelve lo introducido"

# ask_yesno: 's' => 0 (sí), 'n' => 1 (no)
printf 's\n' | ask_yesno "¿Sí?" && assert_eq ok ok "yesno s" || assert_eq ok no "yesno s"
printf 'n\n' | ask_yesno "¿No?" && assert_eq ok no "yesno n" || assert_eq ok ok "yesno n"

# ask_choice devuelve el valor de la opción elegida (por número)
out="$(printf '2\n' | ask_choice "Modo" "lan:Local" "ddns:DDNS" "cf:Cloudflare")"
assert_eq "ddns" "$out" "ask_choice elige por índice"

finish

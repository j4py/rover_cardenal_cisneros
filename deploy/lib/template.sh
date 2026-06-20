#!/usr/bin/env bash
# Render seguro de plantillas con envsubst, limitando a una lista de variables.

# render_template TEMPLATE VARLIST  ('$A $B')  -> escribe a stdout
render_template() {
  local tmpl="$1" vars="${2:-}"
  [ -f "$tmpl" ] || die "Plantilla no encontrada: $tmpl"
  command -v envsubst >/dev/null 2>&1 || die "Falta 'envsubst' (paquete gettext-base)"
  if [ -n "$vars" ]; then
    MSYS_NO_PATHCONV=1 envsubst "$vars" < "$tmpl"
  else
    MSYS_NO_PATHCONV=1 envsubst < "$tmpl"
  fi
}

# render_to TEMPLATE VARLIST OUTFILE  -> render a un archivo (respeta dry-run vía run_root al instalar)
render_to() {
  local tmpl="$1" vars="$2" out="$3"
  if [ "$DRY_RUN" = "1" ]; then
    printf '+ render %s -> %s\n' "$tmpl" "$out"
    return 0
  fi
  render_template "$tmpl" "$vars" > "$out"
}

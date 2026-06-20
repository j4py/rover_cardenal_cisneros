# GPS Telemetry Dashboard — Design Spec
**Date:** 2026-06-17  
**Status:** Approved

## Objetivo

Mostrar telemetría GPS en tiempo real dentro del dashboard de control, expandiendo el panel `SystemStatus` existente. El usuario necesita saber en todo momento si el módulo GPS está encendido, buscando satélites, o con fix adquirido, y cuántos satélites tiene fixeados.

---

## Cambios en ESP32 (`gps.ino`)

### Función `enviarTelemetria()`

Se añaden dos campos al JSON publicado en `rover/telemetry` (cada 1 segundo, sin cambios en intervalo ni tópico):

| Campo | Tipo | Descripción |
|---|---|---|
| `gps_state` | string | Estado del GPS: `"OFF"`, `"SEARCHING"`, `"FIX"` |
| `gps_sats` | int | Número de satélites en uso (`gps.satellites.value()`) |

### Lógica de `gps_state`

```
if (gps.charsProcessed() == 0)       -> "OFF"
else if (!gps.location.isValid())     -> "SEARCHING"
else                                  -> "FIX"
```

### Tamaño del JSON resultante

El StaticJsonDocument<200> actual tiene margen suficiente. Se incrementa a <256> por precaución.

---

## Cambios en Frontend

### `control.html`

- Pasar `gps_state={telemetry.gps_state}` y `gps_sats={telemetry.gps_sats}` como props a `<SystemStatus>`.
- No se modifican los tópicos suscritos ni la lógica de `onMessageArrived`. Los nuevos campos llegan dentro del payload existente de `rover/telemetry` y se absorben automáticamente por el spread `{ ...prev, ...payload }`.

### `components/Dashboard/SystemStatus.js`

- Se elimina el LED de GPS del grid de 3 columnas (MQTT, WIFI, GPS). Queda un grid de 2 columnas (MQTT, WIFI).
- Se añade debajo un bloque `GpsPanel` con:
  - Indicador de estado: icono + texto OFF / SEARCHING / FIX con color semántico.
  - Animación `animate-pulse` solo en estado SEARCHING.
  - Barra de satélites: 12 segmentos, rellenos según `gps_sats`. Oculta en estado OFF.
  - Contador numérico: `N SATs` junto a la barra. `--` en estado OFF.

### Tabla de estilos por estado

| Estado    | Color texto      | Color indicador  | Animación      |
|-----------|-----------------|-----------------|----------------|
| OFF       | text-red-500    | bg-red-700      | ninguna        |
| SEARCHING | text-yellow-400 | bg-yellow-500   | animate-pulse  |
| FIX       | text-green-400  | bg-green-500    | ninguna        |

---

## Flujo de datos

```
gps.ino (TinyGPS++)
  => enviarTelemetria() cada 1s
       => MQTT rover/telemetry { ..., gps_state, gps_sats }
            => control.html onMessageArrived -> setTelemetry({ ...prev, ...payload })
                 => <SystemStatus gps_state={...} gps_sats={...} />
                      => <GpsPanel /> -> render visual
```

---

## Archivos a modificar

1. `ESP32/gps.ino` — función `enviarTelemetria()`
2. `components/Dashboard/SystemStatus.js` — reemplazar LED GPS por GpsPanel
3. `control.html` — pasar nuevas props + bump versión del componente a ?v=1.5

---

## Fuera de alcance

- No se muestran HDOP, altitud ni velocidad GPS.
- No se crean tópicos MQTT nuevos.
- No se modifica el layout general del dashboard.

---

## Notas de implementación

- **Estado inicial:** `gps_state` y `gps_sats` no están en el estado inicial de `telemetry` en `control.html`. El componente `GpsPanel` debe tratar `gps_state === undefined` como `"OFF"` y `gps_sats === undefined` como `0`.
- **`gpsStatus` prop:** Tras eliminar el LED GPS del grid de 3 columnas, la prop `gpsStatus` deja de usarse en `SystemStatus`. Se puede dejar en la firma sin implementar o eliminar limpiamente — queda a criterio de implementación.

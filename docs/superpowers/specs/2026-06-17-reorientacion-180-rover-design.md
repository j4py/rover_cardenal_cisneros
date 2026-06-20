# Reorientación física 180° del rover (cámara al nuevo frente)

- **Fecha:** 2026-06-17
- **Estado:** Diseño aprobado
- **Autor:** Oliver + Claude

## Contexto

Se modificó físicamente el rover: la torre de cámara pasó de un extremo al
otro, de modo que **lo que antes era la parte trasera ahora es el frente**.
El operador redefine las ruedas como **W6→W1, W5→W2, W4→W3** (y simétricamente
W3→W4, W2→W5, W1→W6), y **la derecha pasa a ser izquierda**.

Geométricamente es una **rotación de 180° en yaw** de un rover simétrico de 6
ruedas (rocker-bogie), equivalente a la permutación-involución por pares:

```
W1 ↔ W6     (delantera-izq ↔ trasera-der)
W2 ↔ W5     (media-izq     ↔ media-der)
W3 ↔ W4     (trasera-izq   ↔ delantera-der)
```

con intercambio simultáneo izquierda ↔ derecha.

**Suposición fundamental (confirmada): NO hubo recableado.** Cada motor y servo
sigue conectado a su mismo GPIO; solo cambió qué posición ocupa respecto al
nuevo frente.

## Decisiones tomadas

1. **Estrategia:** remapeo por pares en la **capa de macros** del firmware
   (renombrar el hardware). La cinemática y la lógica de conducción/giro NO se
   modifican. (Alternativa descartada: invertir ejes del joystick, que dejaría
   las etiquetas W1..W6 invertidas respecto a la física.)
2. **Alcance:** firmware principal + sketches de test + `test.html` +
   documentación/notas.
3. **Cámara:** invertir el **pan** en el **firmware** (fuente única de
   verdad).
4. **Coreografía "onda" de `test.html`:** re-derivar los valores µs de forma
   determinista respecto a los nuevos offsets, con verificación en banco.

## Propiedad de seguridad clave

Los offsets de calibración de cada servo **viajan con su servo físico** durante
el remapeo. Por tanto, en marcha recta (todos los servos a 90° = su offset), el
rover queda **mecánicamente centrado y correcto desde el primer arranque**. La
única incógnita que requiere verificación en banco es el **signo de giro**
(dirección) de cada servo y el sentido de cada motor.

## Diseño detallado

### 1. Firmware — remapeo de pines y offsets

Aplica a [`ESP32/Mars_Rover.ino`](../../../ESP32/Mars_Rover.ino) y
[`ESP32/test_diagnostico_hardware.ino`](../../../ESP32/test_diagnostico_hardware.ino).
Cada macro conserva su **nombre**; su **valor** pasa a ser el de su pareja
(orden interno IN1/IN2 preservado):

| Macro | Antes | Después (= pareja) |
|---|---|---|
| `motorW1_IN1` / `motorW1_IN2` | 18 / 5 | **27 / 14** (ex-W6) |
| `motorW6_IN1` / `motorW6_IN2` | 27 / 14 | **18 / 5** (ex-W1) |
| `motorW2_IN1` / `motorW2_IN2` | 4 / 2 | **26 / 25** (ex-W5) |
| `motorW5_IN1` / `motorW5_IN2` | 26 / 25 | **4 / 2** (ex-W2) |
| `motorW3_IN1` / `motorW3_IN2` | 17 / 16 | **33 / 32** (ex-W4) |
| `motorW4_IN1` / `motorW4_IN2` | 33 / 32 | **17 / 16** (ex-W3) |
| `PIN_SERVO_W1` / `OFFSET_SERVO_W1_US` | 19 / 1522 | **13 / 1430** (ex-W6) |
| `PIN_SERVO_W6` / `OFFSET_SERVO_W6_US` | 13 / 1430 | **19 / 1522** (ex-W1) |
| `PIN_SERVO_W3` / `OFFSET_SERVO_W3_US` | 21 / 1523 | **12 / 1408** (ex-W4) |
| `PIN_SERVO_W4` / `OFFSET_SERVO_W4_US` | 12 / 1408 | **21 / 1523** (ex-W3) |

Pines sin cambio: `PIN_SERVO_CAM_TILT` (15), `PIN_STEPPER_STEP` (23),
`PIN_STEPPER_DIR` (22), `PIN_BATTERY` (34).

**Polaridad de tracción:** sale correcta automáticamente con este swap. Como
"nuevo-adelante" = "antiguo-atrás" y la convención espejo izq/der se invierte al
cruzar de lado, aplicar la convención del nuevo lado a los pines de la pareja
produce el sentido físico correcto. Verificado analíticamente para W1, W2 y W6.

### 2. Cinemática

Único cambio en [`ESP32/Mars_Rover.ino`](../../../ESP32/Mars_Rover.ino):
**intercambiar `d2 ↔ d3`** (278 ↔ 301), porque el eje delantero físico nuevo es
el trasero antiguo. `d1` y `d4` (laterales, simétricos bajo el giro) no cambian.
Las funciones `calculateMotorsSpeed()` y `calculateServoAngle()` y los bloques
de giro quedan **sin tocar**.

### 3. Gimbal de cámara (invertir pan en firmware; tilt revertido tras banco)

En [`ESP32/Mars_Rover.ino`](../../../ESP32/Mars_Rover.ino), `callback()`:

- **Pan (`rover/gimbal/yaw`):** negar el sentido en cada
  `camPanStepper.move(...)` (live + test handler) — p. ej. introducir un signo
  `PAN_DIR = -1` aplicado de forma centralizada.
- **Tilt (`rover/gimbal/pitch`):** invertir la dirección vertical negando el
  término de ángulo en la conversión a µs:
  `targetUs = OFFSET_SERVO_CAM_TILT_US - (camTilt - 90) * 10.311`. El comando
  `CENTER` no cambia (a 90° el término es 0).

Con la inversión en firmware, `VideoHud.js` no requiere cambios. En `test.html`
se revisan los botones "Girar Der/Izq" y los presets de tilt para que sus
**etiquetas** concuerden con la nueva física.

### 4. Sketches de test ESP32

- [`test_motores_izquierdos.ino`](../../../ESP32/test_motores_izquierdos.ino) y
  [`test_motores_derechos.ino`](../../../ESP32/test_motores_derechos.ino):
  reasignar pines según la tabla y actualizar etiquetas Delantera/Trasera/Lado.
  Dado que uno es "lado izquierdo" y otro "lado derecho", en la práctica se
  **intercambian los grupos de pines entre ambos archivos**.
- [`test_diagnostico_hardware.ino`](../../../ESP32/test_diagnostico_hardware.ino):
  misma tabla de pines/offsets que el firmware + comentarios de posición y los
  rótulos en `nombres[]`/sweeps de `testServosDireccion()`.

### 5. UI de test ([`test.html`](../../../test.html))

- **Tarjetas de motor:** sin cambios. Al remapear en firmware, "Motor FL (W1)"
  → id 1 → `motorW1` → vuelve a ser físicamente la rueda delantera-izquierda.
- **Tarjetas de servo (`ServoCard`):** intercambiar las props `offset`
  (W1↔W6: 1522↔1430; W3↔W4: 1523↔1408) para que el centrado sea correcto.
- **Coreografía "onda":** re-derivar de forma determinista los µs hardcodeados
  por servo (preservando el mismo delta respecto al nuevo offset de cada servo)
  y reordenar/intercambiar IDs de motor/servo para que el gait quede coherente
  con el nuevo frente. Pieza más delicada → verificación en banco.
- **Botones pan / presets tilt:** alinear etiquetas con la inversión del gimbal.
- **Cabeceras** "(Lateral Derecho/Izquierdo ESP32)": ajustar el paréntesis (el
  grupo de GPIO de cada lado se intercambió).

### 6. Documentación / notas (solo etiquetado, sin lógica)

Actualizar mapa GPIO↔rueda y layout izq/der en: [`README.md`](../../../README.md),
[`hardware/conexiones.html`](../../../hardware/conexiones.html),
[`hardware/traccion.html`](../../../hardware/traccion.html),
[`hardware/electronica.html`](../../../hardware/electronica.html),
[`sensores/camara.html`](../../../sensores/camara.html) y notas de defensa en
[`_notas/`](../../../_notas/).

## Plan de verificación en banco (ruedas en el aire)

1. **Recto:** joystick adelante → las 6 ruedas ruedan hacia el nuevo frente. Si
   alguna gira al revés, intercambiar su `IN1/IN2`.
2. **Dirección:** steering izq/der → cada servo apunta al lado correcto. Si
   alguno invierte, negar el término de ángulo de ese servo en los 4 bloques de
   giro (recto a 90° no se ve afectado).
3. **Gimbal:** confirmar pan izq/der y tilt arriba/abajo respecto a la imagen.
4. Reajustar offsets de servo solo si el cero mecánico se hubiera movido.

## Riesgos

- **Signo de dirección de los servos** (incógnita principal): se resuelve en
  banco invirtiendo el término de ángulo del servo afectado. La marcha recta es
  siempre segura.
- **Coreografía:** demo cosmética; un error solo la hace verse "rara", no es
  peligroso para el hardware.
- Se asume que los offsets siguen válidos (no se recalibró el cero mecánico de
  cada servo).

## Fuera de alcance

- Recalibración mecánica de servos.
- Cambios de lógica de cinemática/conducción (se reutiliza intacta).
- Cualquier modificación de red/MQTT/OTA/telemetría.

## Actualización post-verificación en banco (2026-06-17)

Tras cargar el firmware y probar en banco: el **pan** sí requería inversión (`PAN_DIR -1`, correcto). El **tilt NO** necesitaba inversión — el eje de cabeceo no se ve afectado por el giro 180° en yaw. Se revirtió el signo del tilt en `Mars_Rover.ino`, `test_diagnostico_hardware.ino` y `test.html` (commit `dcdf2e1`). El resto del remapeo se confirmó correcto en hardware.

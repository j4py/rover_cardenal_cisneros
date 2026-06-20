# Reorientación 180° del rover — Plan de Implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Adaptar firmware, sketches de test, UI de test y documentación al giro físico de 180° del rover (cámara al nuevo frente), conservando intacta la cinemática.

**Architecture:** Remapeo en la capa de macros (permutación-involución W1↔W6, W2↔W5, W3↔W4) sin tocar la lógica de conducción/giro. Inversión del gimbal (pan+tilt) en firmware. El resto son ajustes de calibración (offsets), geometría (d2↔d3) y etiquetado.

**Tech Stack:** Arduino C++ (ESP32), HTML + React (Babel in-browser), MQTT.

**Referencia canónica del remapeo** (úsala en todas las tareas):

| Nombre lógico | Motor IN1/IN2 (nuevo) | Servo pin (nuevo) | Servo offset µs (nuevo) | Origen físico |
|---|---|---|---|---|
| W1 (delantera-izq) | 27 / 14 | 13 | 1430 | ex-W6 |
| W2 (media-izq)     | 26 / 25 | — | — | ex-W5 |
| W3 (trasera-izq)   | 33 / 32 | 12 | 1408 | ex-W4 |
| W4 (delantera-der) | 17 / 16 | 21 | 1523 | ex-W3 |
| W5 (media-der)     | 4 / 2   | — | — | ex-W2 |
| W6 (trasera-der)   | 18 / 5  | 19 | 1522 | ex-W1 |

Sin cambio: `PIN_BATTERY` 34, `PIN_SERVO_CAM_TILT` 15, `PIN_STEPPER_STEP` 23, `PIN_STEPPER_DIR` 22, `OFFSET_SERVO_CAM_TILT_US` 1472.

**Nota sobre verificación:** no hay framework de tests automatizados (sketches Arduino + HTML estático). La "verificación" de cada tarea es (a) revisión por `grep`/diff y (b) la verificación en banco (Task 9). Donde sea posible se incluye un `grep` de comprobación.

---

### Task 0: Crear rama de trabajo

**Files:** ninguno (operación git)

- [ ] **Step 1: Crear y cambiar a la rama**

```bash
git checkout -b reorientacion-180-rover
```

- [ ] **Step 2: Verificar rama limpia**

Run: `git status`
Expected: `On branch reorientacion-180-rover` y working tree limpio (salvo la spec/plan ya creados en `docs/superpowers/`).

- [ ] **Step 3: Commit de spec + plan**

```bash
git add docs/superpowers/specs/2026-06-17-reorientacion-180-rover-design.md docs/superpowers/plans/2026-06-17-reorientacion-180-rover.md
git commit -m "docs: spec y plan de reorientación 180° del rover"
```

---

### Task 1: Firmware — remapear pines y offsets de servo

**Files:**
- Modify: `ESP32/Mars_Rover.ino:44-75`

- [ ] **Step 1: Reemplazar el bloque de pines + offsets**

Sustituir desde `// ------------------ CONFIGURACIÓN DE PINES ESP32` hasta la línea `#define OFFSET_SERVO_CAM_TILT_US 1472 // Antiguo 90°` por:

```cpp
// ------------------ CONFIGURACIÓN DE PINES ESP32
// Tras el giro físico de 180° del rover, el header IZQUIERDO del ESP32 pasa a
// alimentar el lado DERECHO del rover y viceversa. Los nombres lógicos W1..W6
// se conservan (respecto al NUEVO frente); solo se reasignan los GPIO según la
// permutación W1<->W6, W2<->W5, W3<->W4.

// Lado Izquierdo del Rover (cableado al header DERECHO del ESP32)
#define motorW1_IN1 27 // ex-W6
#define motorW1_IN2 14
#define motorW2_IN1 26 // ex-W5
#define motorW2_IN2 25
#define motorW3_IN1 33 // ex-W4
#define motorW3_IN2 32
#define PIN_SERVO_W1 13 // ex-W6
#define PIN_SERVO_W3 12 // ex-W4
#define PIN_BATTERY 34

// Lado Derecho del Rover (cableado al header IZQUIERDO del ESP32)
#define motorW4_IN1 17 // ex-W3
#define motorW4_IN2 16
#define motorW5_IN1 4 // ex-W2
#define motorW5_IN2 2
#define motorW6_IN1 18 // ex-W1
#define motorW6_IN2 5
#define PIN_SERVO_W4 21 // ex-W3
#define PIN_SERVO_W6 19 // ex-W1
#define PIN_SERVO_CAM_TILT 15
#define PIN_STEPPER_STEP 23
#define PIN_STEPPER_DIR 22

// ------------------ CALIBRACIÓN DE SERVOS EN MICROSEGUNDOS (PUNTOS 0)
// ------------------ (los offsets viajan con su servo físico)
#define OFFSET_SERVO_W1_US 1430       // ex-W6
#define OFFSET_SERVO_W3_US 1408       // ex-W4
#define OFFSET_SERVO_W4_US 1523       // ex-W3
#define OFFSET_SERVO_W6_US 1522       // ex-W1
#define OFFSET_SERVO_CAM_TILT_US 1472 // 90° (sin cambio)
```

- [ ] **Step 2: Verificar que los 12 GPIO de motor siguen presentes sin duplicados**

Run: `grep -E "#define motorW[1-6]_IN[12]" ESP32/Mars_Rover.ino | grep -oE "[0-9]+$" | sort -n | uniq -d`
Expected: salida vacía (ningún GPIO duplicado).

- [ ] **Step 3: Verificar el mapeo nuevo**

Run: `grep -E "#define (motorW1_IN1|motorW6_IN1|PIN_SERVO_W1|OFFSET_SERVO_W1_US)" ESP32/Mars_Rover.ino`
Expected: `motorW1_IN1 27`, `motorW6_IN1 18`, `PIN_SERVO_W1 13`, `OFFSET_SERVO_W1_US 1430`.

- [ ] **Step 4: Commit**

```bash
git add ESP32/Mars_Rover.ino
git commit -m "fix(firmware): remapear pines/offsets de ruedas tras giro 180°"
```

---

### Task 2: Firmware — intercambiar geometría d2 ↔ d3

**Files:**
- Modify: `ESP32/Mars_Rover.ino:100-104`

- [ ] **Step 1: Editar las constantes de geometría**

Reemplazar:

```cpp
// Geometría del rover en mm
float d1 = 271;
float d2 = 278;
float d3 = 301;
float d4 = 304;
```

por:

```cpp
// Geometría del rover en mm
// d2/d3 intercambiados tras el giro 180°: el eje delantero nuevo es el trasero antiguo.
float d1 = 271;
float d2 = 301;
float d3 = 278;
float d4 = 304;
```

- [ ] **Step 2: Verificar**

Run: `grep -E "float d[23] =" ESP32/Mars_Rover.ino`
Expected: `float d2 = 301;` y `float d3 = 278;`

- [ ] **Step 3: Commit**

```bash
git add ESP32/Mars_Rover.ino
git commit -m "fix(firmware): intercambiar d2/d3 por reorientación frontal"
```

---

### Task 3: Firmware — invertir gimbal (pan + tilt)

**Files:**
- Modify: `ESP32/Mars_Rover.ino` (define nuevo + 3 ediciones puntuales)

- [ ] **Step 1: Añadir el define `PAN_DIR`**

Justo después de la línea `#define OFFSET_SERVO_CAM_TILT_US 1472 // 90° (sin cambio)` (creada en Task 1), insertar:

```cpp

// ------------------ INVERSIÓN DE GIMBAL (cámara remontada tras giro 180°)
#define PAN_DIR -1 // Invierte el sentido del paneo (pan) de la cámara
```

- [ ] **Step 2: Invertir el pan en el control en vivo (`rover/gimbal/yaw`)**

Reemplazar:

```cpp
    int relativeSteps = message.toInt();
    if (relativeSteps != 0) {
      camPanStepper.move(relativeSteps);
    }
```

por:

```cpp
    int relativeSteps = message.toInt();
    if (relativeSteps != 0) {
      camPanStepper.move(PAN_DIR * relativeSteps);
    }
```

- [ ] **Step 3: Invertir el pan en el handler de test (movimiento relativo)**

Reemplazar:

```cpp
          camPanStepper.move(steps * dir);
```

por:

```cpp
          camPanStepper.move(PAN_DIR * steps * dir);
```

- [ ] **Step 4: Invertir el tilt en el control en vivo (`rover/gimbal/pitch`)**

Reemplazar:

```cpp
        int targetUs = OFFSET_SERVO_CAM_TILT_US + (camTilt - 90.0) * 10.311;
        servoCamTilt.startEaseTo(targetUs);
```

por:

```cpp
        int targetUs = OFFSET_SERVO_CAM_TILT_US - (camTilt - 90.0) * 10.311;
        servoCamTilt.startEaseTo(targetUs);
```

- [ ] **Step 5: Verificar**

Run: `grep -nE "PAN_DIR|OFFSET_SERVO_CAM_TILT_US - \(camTilt" ESP32/Mars_Rover.ino`
Expected: 1 define `PAN_DIR -1`, 2 usos `PAN_DIR *`, y la línea de tilt con el signo `-`.

- [ ] **Step 6: Commit**

```bash
git add ESP32/Mars_Rover.ino
git commit -m "fix(firmware): invertir pan y tilt del gimbal tras remonte de cámara"
```

---

### Task 4: Sketch de diagnóstico — remapear pines/offsets + tilt

**Files:**
- Modify: `ESP32/test_diagnostico_hardware.ino:36-66` (pines/offsets) y `:577-586` (sweep tilt)

- [ ] **Step 1: Reemplazar el bloque de pines + offsets**

Sustituir desde `// ------------------ CONFIGURACIÓN DE PINES ESP32 ------------------` hasta `#define OFFSET_SERVO_CAM_TILT_US 1472` por:

```cpp
// ------------------ CONFIGURACIÓN DE PINES ESP32 ------------------
// Remapeado tras giro físico de 180° (W1<->W6, W2<->W5, W3<->W4).
// Lado Izquierdo del Rover (header DERECHO del ESP32)
#define motorW1_IN1 27 // ex-W6
#define motorW1_IN2 14
#define motorW2_IN1 26 // ex-W5
#define motorW2_IN2 25
#define motorW3_IN1 33 // ex-W4
#define motorW3_IN2 32
#define PIN_SERVO_W1 13 // ex-W6
#define PIN_SERVO_W3 12 // ex-W4
#define PIN_BATTERY 34

// Lado Derecho del Rover (header IZQUIERDO del ESP32)
#define motorW4_IN1 17 // ex-W3
#define motorW4_IN2 16
#define motorW5_IN1 4 // ex-W2
#define motorW5_IN2 2
#define motorW6_IN1 18 // ex-W1
#define motorW6_IN2 5
#define PIN_SERVO_W4 21 // ex-W3
#define PIN_SERVO_W6 19 // ex-W1
#define PIN_SERVO_CAM_TILT 15
#define PIN_STEPPER_STEP 23
#define PIN_STEPPER_DIR 22

// ------------------ CALIBRACIÓN DE SERVOS ------------------
#define OFFSET_SERVO_W1_US 1430 // ex-W6
#define OFFSET_SERVO_W3_US 1408 // ex-W4
#define OFFSET_SERVO_W4_US 1523 // ex-W3
#define OFFSET_SERVO_W6_US 1522 // ex-W1
#define OFFSET_SERVO_CAM_TILT_US 1472
```

- [ ] **Step 2: Invertir el sentido del sweep de tilt**

Reemplazar:

```cpp
  int targetUs35 = OFFSET_SERVO_CAM_TILT_US + (35 - 90) * 10.311;
```

por:

```cpp
  int targetUs35 = OFFSET_SERVO_CAM_TILT_US - (35 - 90) * 10.311;
```

y reemplazar:

```cpp
  int targetUs165 = OFFSET_SERVO_CAM_TILT_US + (165 - 90) * 10.311;
```

por:

```cpp
  int targetUs165 = OFFSET_SERVO_CAM_TILT_US - (165 - 90) * 10.311;
```

- [ ] **Step 3: Verificar sin GPIO duplicados**

Run: `grep -E "#define motorW[1-6]_IN[12]" ESP32/test_diagnostico_hardware.ino | grep -oE "[0-9]+$" | sort -n | uniq -d`
Expected: salida vacía.

- [ ] **Step 4: Commit**

```bash
git add ESP32/test_diagnostico_hardware.ino
git commit -m "fix(test): remapear pines/offsets y tilt en sketch de diagnóstico"
```

---

### Task 5: Sketches de motores — intercambiar grupos de pines

Los dos sketches prueban, respectivamente, las 3 ruedas del lado izquierdo y derecho del rover. Tras el giro, el lado izquierdo del rover usa los GPIO que antes eran del derecho y viceversa. Se normaliza el orden IN1/IN2 para que coincida con el firmware principal.

**Files:**
- Modify: `ESP32/test_motores_izquierdos.ino:17-23`
- Modify: `ESP32/test_motores_derechos.ino:7-13`

- [ ] **Step 1: Reemplazar pines en `test_motores_izquierdos.ino` (ruedas W1, W2, W3)**

Reemplazar:

```cpp
// --- CONFIGURACIÓN DE PINES (Lado Izquierdo) ---
#define motorW1_IN1 5  // Rueda 1 - Delantera Izquierda
#define motorW1_IN2 18
#define motorW2_IN1 2  // Rueda 2 - Medio Izquierda
#define motorW2_IN2 4
#define motorW3_IN1 16 // Rueda 3 - Trasera Izquierda
#define motorW3_IN2 17
```

por:

```cpp
// --- CONFIGURACIÓN DE PINES (Lado Izquierdo del Rover, tras giro 180°) ---
#define motorW1_IN1 27 // Rueda 1 - Delantera Izquierda (ex-W6)
#define motorW1_IN2 14
#define motorW2_IN1 26 // Rueda 2 - Medio Izquierda (ex-W5)
#define motorW2_IN2 25
#define motorW3_IN1 33 // Rueda 3 - Trasera Izquierda (ex-W4)
#define motorW3_IN2 32
```

- [ ] **Step 2: Reemplazar pines en `test_motores_derechos.ino` (ruedas W4, W5, W6)**

Reemplazar:

```cpp
// --- CONFIGURACIÓN DE PINES (Lado Derecho) ---
#define motorW4_IN1 32 // Rueda 4 - Delantera Derecha
#define motorW4_IN2 33
#define motorW5_IN1 25 // Rueda 5 - Medio Derecha
#define motorW5_IN2 26
#define motorW6_IN1 27 // Rueda 6 - Trasera Derecha
#define motorW6_IN2 14
```

por:

```cpp
// --- CONFIGURACIÓN DE PINES (Lado Derecho del Rover, tras giro 180°) ---
#define motorW4_IN1 17 // Rueda 4 - Delantera Derecha (ex-W3)
#define motorW4_IN2 16
#define motorW5_IN1 4  // Rueda 5 - Medio Derecha (ex-W2)
#define motorW5_IN2 2
#define motorW6_IN1 18 // Rueda 6 - Trasera Derecha (ex-W1)
#define motorW6_IN2 5
```

- [ ] **Step 3: Actualizar los números de GPIO citados en los `printLog`**

En `test_motores_derechos.ino`, actualizar las cadenas de log de cada prueba:
- "Rueda 4 (Delantera Derecha - GPIO 32/33)" → "GPIO 17/16"
- "Rueda 5 (Medio Derecha - GPIO 25/26)" → "GPIO 4/2"
- "Rueda 6 (Trasera Derecha - GPIO 27/14)" → "GPIO 18/5"

En `test_motores_izquierdos.ino`, actualizar el comentario de cabecera y logs:
- "Rueda 1 (Delantera Izquierda): GPIO 5 (IN1) y GPIO 18 (IN2)" → "GPIO 27 (IN1) y GPIO 14 (IN2)"
- "Rueda 2 (Medio Izquierda): GPIO 2 (IN1) y GPIO 4 (IN2)" → "GPIO 26 (IN1) y GPIO 25 (IN2)"
- "Rueda 3 (Trasera Izquierda): GPIO 16 (IN1) y GPIO 17 (IN2)" → "GPIO 33 (IN1) y GPIO 32 (IN2)"
- "[PRUEBA 1] Rueda 1 (Delantera Izquierda - GPIO 5/18)" → "GPIO 27/14"
- "[PRUEBA 2] Rueda 2 (Medio Izquierda - GPIO 2/4)" → "GPIO 26/25"
- "[PRUEBA 3] Rueda 3 (Trasera Izquierda - GPIO 16/17)" → "GPIO 33/32"

- [ ] **Step 4: Verificar**

Run: `grep -nE "GPIO (5|18|2|4|16|17|32|33|25|26|27|14)/" ESP32/test_motores_*.ino`
Expected: solo aparecen los nuevos pares (17/16, 4/2, 18/5, 27/14, 26/25, 33/32); ningún par antiguo huérfano. Revisar visualmente.

- [ ] **Step 5: Commit**

```bash
git add ESP32/test_motores_izquierdos.ino ESP32/test_motores_derechos.ino
git commit -m "fix(test): intercambiar grupos de pines en sketches de motores"
```

---

### Task 6: UI de test — offsets de servo + etiquetas de tilt

Las tarjetas de motor NO cambian (al remapear en firmware, id 1 vuelve a ser la rueda delantera-izquierda). Las cabeceras "Lado Izquierdo (Lateral Derecho ESP32)" / "Lado Derecho (Lateral Izquierdo ESP32)" tampoco cambian: siguen siendo correctas porque el grupo de GPIO de cada lado del ESP32 es fijo y ahora alimenta el lado opuesto del rover.

**Files:**
- Modify: `test.html:798,813,877,892` (props `offset`) y `:862` (presetLabels de tilt)

- [ ] **Step 1: Intercambiar los offsets de las tarjetas de servo de dirección**

- Línea 798 (`ServoCard id={1}`): `offset={1522}` → `offset={1430}`
- Línea 813 (`ServoCard id={3}`): `offset={1523}` → `offset={1408}`
- Línea 877 (`ServoCard id={4}`): `offset={1408}` → `offset={1523}`
- Línea 892 (`ServoCard id={6}`): `offset={1430}` → `offset={1522}`

- [ ] **Step 2: Corregir las etiquetas de preset del tilt (invertido)**

Línea 862, reemplazar:

```jsx
presetLabels={["(Abajo)", "(Frente)", "(Arriba)"]}
```

por:

```jsx
presetLabels={["(Arriba)", "(Frente)", "(Abajo)"]}
```

- [ ] **Step 3: Verificar**

Run: `grep -nE "ServoCard id=\{[1346]\}|presetLabels=\{\[\"\(Arriba\)" test.html`
Expected: id={1} offset 1430, id={3} offset 1408, id={4} offset 1523, id={6} offset 1522, y la nueva presetLabels.

- [ ] **Step 4: Commit**

```bash
git add test.html
git commit -m "fix(ui): intercambiar offsets de servo y etiquetas de tilt en test.html"
```

---

### Task 7: UI de test — re-derivar la coreografía "onda"

Transformación determinista: para cada servo, `nuevoUs = nuevoOffset + (viejoUs − viejoOffset)`. Equivale a intercambiar los juegos de valores entre servos emparejados (1↔6, 3↔4) e invertir el cam (1172↔1772). Las palabras "izquierda/derecha" en los logs se invierten (el giro 180° intercambia lados); quedan marcadas para ajuste fino en banco.

**Files:**
- Modify: `test.html:349-498` (funciones `runIda` y `runVuelta`) y `:511-512` (cierre t24)

- [ ] **Step 1: Reemplazar `runIda` completa (líneas 349-421)**

```jsx
        const runIda = (times, waveN) => {
          // Step 0 — W1 (doubles as cleanup from previous vuelta when waveN > 1)
          const s0 = setTimeout(() => {
            if (waveN > 1) {
              addChoroLog(`[ESTRUCTURA] Centrando servos W1 y W3 de la onda ${waveN - 1}.`);
              addChoroLog(`[TRACCIÓN] Deteniendo W1 (Vuelta). Onda de vuelta ${waveN - 1} completada.`);
              sendCmd({ type: "servo", id: "1", us: 1430 });
              sendCmd({ type: "servo", id: "3", us: 1408 });
              sendCmd({ type: "motor", id: 1, dir: 0 });
            }
            addChoroLog(`[ESTRUCTURA] Onda de ida ${waveN}: Servo W1 girando a la derecha (1130µs).`);
            addChoroLog(`[TRACCIÓN] Girando motor tracción FL (W1) Adelante.`);
            sendCmd({ type: "servo", id: "1", us: 1130 });
            sendCmd({ type: "motor", id: 1, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w1: 'TEST_IDA', w3: waveN > 1 ? 'OK' : prev.subsystems.w3, propulsion: 'W1_ADELANTE' } }));
          }, times[0]);
          choroTimersRef.current.push(s0);

          // Step 1 — W2
          const s1 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Manteniendo servo W1 girado.");
            addChoroLog("[TRACCIÓN] Deteniendo W1. Girando motor central ML (W2) Adelante.");
            sendCmd({ type: "motor", id: 1, dir: 0 });
            sendCmd({ type: "motor", id: 2, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, propulsion: 'W2_ADELANTE' } }));
          }, times[1]);
          choroTimersRef.current.push(s1);

          // Step 2 — W3
          const s2 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Servo W3 girando a la derecha (1108µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W2. Girando motor tracción BL (W3) Adelante.");
            sendCmd({ type: "motor", id: 2, dir: 0 });
            sendCmd({ type: "servo", id: "3", us: 1108 });
            sendCmd({ type: "motor", id: 3, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w3: 'TEST_IDA', propulsion: 'W3_ADELANTE' } }));
          }, times[2]);
          choroTimersRef.current.push(s2);

          // Step 3 — W4
          const s3 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Centrando servo W1. Servo W4 girando a la derecha (1223µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W3. Girando motor tracción FR (W4) Adelante.");
            sendCmd({ type: "servo", id: "1", us: 1430 });
            sendCmd({ type: "motor", id: 3, dir: 0 });
            sendCmd({ type: "servo", id: "4", us: 1223 });
            sendCmd({ type: "motor", id: 4, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w1: 'OK', w4: 'TEST_IDA', propulsion: 'W4_ADELANTE' } }));
          }, times[3]);
          choroTimersRef.current.push(s3);

          // Step 4 — W5
          const s4 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Manteniendo servo W4 girado.");
            addChoroLog("[TRACCIÓN] Deteniendo W4. Girando motor central MR (W5) Adelante.");
            sendCmd({ type: "motor", id: 4, dir: 0 });
            sendCmd({ type: "motor", id: 5, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, propulsion: 'W5_ADELANTE' } }));
          }, times[4]);
          choroTimersRef.current.push(s4);

          // Step 5 — W6
          const s5 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Centrando servo W3. Servo W6 girando a la derecha (1222µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W5. Girando motor tracción BR (W6) Adelante.");
            sendCmd({ type: "servo", id: "3", us: 1408 });
            sendCmd({ type: "motor", id: 5, dir: 0 });
            sendCmd({ type: "servo", id: "6", us: 1222 });
            sendCmd({ type: "motor", id: 6, dir: 1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w3: 'OK', w6: 'TEST_IDA', propulsion: 'W6_ADELANTE' } }));
          }, times[5]);
          choroTimersRef.current.push(s5);
        };
```

- [ ] **Step 2: Reemplazar `runVuelta` completa (líneas 424-498)**

```jsx
        const runVuelta = (times, waveN) => {
          // Step 0 — W6 (cleanup + start return)
          const s0 = setTimeout(() => {
            addChoroLog(`[ESTRUCTURA] Centrando servos W4 y W6 de la onda ${waveN}.`);
            addChoroLog(`[TRACCIÓN] Deteniendo W6 (Ida). Onda de ida ${waveN} completada.`);
            addChoroLog(`[ESTRUCTURA] Onda de vuelta ${waveN}: Servo W6 girando a la izquierda (1822µs).`);
            addChoroLog("[TRACCIÓN] Girando motor tracción BR (W6) Atrás.");
            addChoroLog("[CÁMARA] Bajando pitch de cámara.");
            sendCmd({ type: "servo", id: "4", us: 1523 });
            sendCmd({ type: "servo", id: "6", us: 1522 });
            sendCmd({ type: "motor", id: 6, dir: 0 });
            sendCmd({ type: "servo", id: "6", us: 1822 });
            sendCmd({ type: "motor", id: 6, dir: -1, speed: 180, duration: 400, stopOthers: false });
            sendCmd({ type: "servo", id: "cam", us: 1772 });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w4: 'OK', w6: 'TEST_VUELTA', cam: 'TILT_ABAJO', propulsion: 'W6_ATRÁS' } }));
          }, times[0]);
          choroTimersRef.current.push(s0);

          // Step 1 — W5
          const s1 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Manteniendo servo W6 girado.");
            addChoroLog("[TRACCIÓN] Deteniendo W6. Girando motor central MR (W5) Atrás.");
            sendCmd({ type: "motor", id: 6, dir: 0 });
            sendCmd({ type: "motor", id: 5, dir: -1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, propulsion: 'W5_ATRÁS' } }));
          }, times[1]);
          choroTimersRef.current.push(s1);

          // Step 2 — W4
          const s2 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Servo W4 girando a la izquierda (1823µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W5. Girando motor tracción FR (W4) Atrás.");
            sendCmd({ type: "motor", id: 5, dir: 0 });
            sendCmd({ type: "servo", id: "4", us: 1823 });
            sendCmd({ type: "motor", id: 4, dir: -1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w4: 'TEST_VUELTA', propulsion: 'W4_ATRÁS' } }));
          }, times[2]);
          choroTimersRef.current.push(s2);

          // Step 3 — W3
          const s3 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Centrando servo W6. Servo W3 girando a la izquierda (1708µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W4. Girando motor tracción BL (W3) Atrás.");
            sendCmd({ type: "servo", id: "6", us: 1522 });
            sendCmd({ type: "motor", id: 4, dir: 0 });
            sendCmd({ type: "servo", id: "3", us: 1708 });
            sendCmd({ type: "motor", id: 3, dir: -1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w6: 'OK', w3: 'TEST_VUELTA', propulsion: 'W3_ATRÁS' } }));
          }, times[3]);
          choroTimersRef.current.push(s3);

          // Step 4 — W2
          const s4 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Manteniendo servo W3 girado.");
            addChoroLog("[TRACCIÓN] Deteniendo W3. Girando motor central ML (W2) Atrás.");
            sendCmd({ type: "motor", id: 3, dir: 0 });
            sendCmd({ type: "motor", id: 2, dir: -1, speed: 180, duration: 400, stopOthers: false });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, propulsion: 'W2_ATRÁS' } }));
          }, times[4]);
          choroTimersRef.current.push(s4);

          // Step 5 — W1
          const s5 = setTimeout(() => {
            addChoroLog("[ESTRUCTURA] Centrando servo W4. Servo W1 girando a la izquierda (1730µs).");
            addChoroLog("[TRACCIÓN] Deteniendo W2. Girando motor tracción FL (W1) Atrás.");
            addChoroLog("[CÁMARA] Subiendo pitch de cámara.");
            sendCmd({ type: "servo", id: "4", us: 1523 });
            sendCmd({ type: "motor", id: 2, dir: 0 });
            sendCmd({ type: "servo", id: "1", us: 1730 });
            sendCmd({ type: "motor", id: 1, dir: -1, speed: 180, duration: 400, stopOthers: false });
            sendCmd({ type: "servo", id: "cam", us: 1172 });
            setChoroState(prev => ({ ...prev, subsystems: { ...prev.subsystems, w4: 'OK', w1: 'TEST_VUELTA', cam: 'TILT_ARRIBA', propulsion: 'W1_ATRÁS' } }));
          }, times[5]);
          choroTimersRef.current.push(s5);
        };
```

- [ ] **Step 3: Actualizar los servos en el cierre t24 (líneas 511-512)**

Reemplazar:

```jsx
          sendCmd({ type: "servo", id: "1", us: 1522 });
          sendCmd({ type: "servo", id: "3", us: 1523 });
```

por:

```jsx
          sendCmd({ type: "servo", id: "1", us: 1430 });
          sendCmd({ type: "servo", id: "3", us: 1408 });
```

(La línea `sendCmd({ type: "servo", id: "cam", us: 1472 });` no cambia: el centro del tilt es 1472.)

- [ ] **Step 4: Verificar que no quedan valores antiguos de servo en la coreografía**

Run: `grep -nE "id: \"(1|3|4|6)\", us: (1522|1523|1222|1822|1223|1823|1408|1108|1708|1430|1130|1730)" test.html`
Expected: salida vacía (todos los us de los servos 1/3/4/6 dentro de la coreografía ya son los nuevos valores). Revisar visualmente las funciones.

- [ ] **Step 5: Commit**

```bash
git add test.html
git commit -m "fix(ui): re-derivar coreografía 'onda' para el nuevo frente"
```

---

### Task 8: Documentación y notas

Aplicar la **referencia canónica del remapeo** (cabecera del plan) a la documentación. Es relabeling descriptivo, sin lógica.

**Files (revisar y actualizar mapas GPIO↔rueda y layout izq/der):**
- `README.md`
- `hardware/conexiones.html`
- `hardware/traccion.html`
- `hardware/electronica.html`
- `sensores/camara.html`
- `_notas/Hardware_y_Software.md`, `_notas/Problemas_encontrados_y_soluciones_aplicadas.md`, `_notas/preguntas_defensa_tribunal.md`, `_notas/guion_defensa_tribunal.md`, `_notas/contenido_diapositivas.md`, `_notas/contexto_proyecto.md`

- [ ] **Step 1: Localizar las menciones de pines/ruedas**

Run: `grep -rnE "GPIO ?(18|5|4|2|17|16|33|32|26|25|27|14|19|21|12|13)|W[1-6]|Delanter|Traser|Izquierd|Derech" README.md hardware/ sensores/ _notas/ --include=*.md --include=*.html`

- [ ] **Step 2: Actualizar cada documento**

Para cada coincidencia que describa qué GPIO corresponde a qué rueda, o el lado de una rueda, ajustarla a la referencia canónica. Donde se mencione el pan/tilt de la cámara, anotar que están invertidos respecto al frente nuevo. No tocar narrativa no afectada (cinemática, MQTT, red).

- [ ] **Step 3: Verificar que no quedan pares GPIO↔rueda contradictorios**

Run: `grep -rnE "W1.*GPIO ?18|W6.*GPIO ?27|W4.*GPIO ?33|W3.*GPIO ?17" README.md hardware/ sensores/ _notas/`
Expected: salida vacía (ya no hay asociaciones del mapeo antiguo).

- [ ] **Step 4: Commit**

```bash
git add README.md hardware/ sensores/ _notas/
git commit -m "docs: actualizar mapa GPIO/ruedas y layout tras giro 180°"
```

---

### Task 9: Verificación en banco (hardware, ruedas en el aire)

**Files:** ninguno (verificación manual). Cargar `Mars_Rover.ino` por OTA y abrir `control.html` + `test.html`.

> **SEGURIDAD:** rover suspendido o con espacio libre. La marcha recta es segura desde el inicio (servos centrados). Las incógnitas son los signos de giro.

- [ ] **Step 1: Tracción recta**

Joystick adelante a velocidad baja. Las 6 ruedas deben rodar hacia el NUEVO frente (lado cámara).
- Si una rueda gira al revés: intercambiar su `IN1`/`IN2` en `ESP32/Mars_Rover.ino` y recargar.

- [ ] **Step 2: Dirección**

Mover steering a izquierda y derecha. Cada servo de esquina debe orientar su rueda hacia el lado indicado y el rover debe girar en ese sentido.
- Si un servo orienta al lado contrario: en los 4 bloques de giro de `loop()`, negar el término de ese servo (p. ej. `90 + thetaInnerFront` → `90 - thetaInnerFront`). El recto (90°) no se ve afectado.

- [ ] **Step 3: Gimbal**

Pan L/R debe mover la imagen hacia el lado indicado; Tilt UP/DOWN debe subir/bajar la imagen.
- Si pan invertido: cambiar `PAN_DIR` a `+1`.
- Si tilt invertido: revertir el signo en la línea de `targetUs` del pitch.

- [ ] **Step 4: Test panel + coreografía**

En `test.html`, probar cada tarjeta de motor/servo (deben corresponder a la posición física rotulada) y lanzar la coreografía "onda"; ajustar finamente palabras izq/der o µs si algún servo desentona.

- [ ] **Step 5: Commit de los ajustes de banco (si los hubo)**

```bash
git add -A
git commit -m "fix: ajustes de banco tras verificación de reorientación 180°"
```

---

## Self-Review (cobertura de la spec)

- Remapeo pines/offsets firmware → Task 1 ✓
- d2↔d3 → Task 2 ✓
- Gimbal pan+tilt en firmware → Task 3 ✓
- Sketch diagnóstico → Task 4 ✓
- Sketches de motores → Task 5 ✓
- test.html offsets servo + tilt labels (cabeceras sin cambio, justificado) → Task 6 ✓
- Coreografía "onda" re-derivada → Task 7 ✓
- Documentación/notas → Task 8 ✓
- Plan de verificación en banco (signos de motor/servo, gimbal) → Task 9 ✓

Consistencia de valores: el mapa de offsets (W1=1430, W3=1408, W4=1523, W6=1522) es idéntico en Task 1, 4 y 6, y los µs de la coreografía (Task 7) derivan de él. `PAN_DIR` definido en Task 3 y usado solo allí.

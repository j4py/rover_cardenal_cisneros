/*
 *   Mars Rover - ESP32 Hardware Diagnostics Utility with Web Serial Console
 *   ========================================================================
 *   Esta herramienta interactiva permite verificar de forma física e de fondo
 *   el correcto funcionamiento de todos los pines y componentes del Rover.
 *   
 *   ¡Ahora con Monitor Serie por WiFi!
 *   Puedes interactuar con el rover desde cualquier navegador web abriendo:
 *   http://<IP-DEL-ROVER>/serial o http://perseverance-rover-esp32.local/serial
 *   
 *   También sigue disponible mediante cable USB (Monitor Serie a 115200 baudios).
 */

#include "driver/gpio.h"
#include <ESP32Servo.h>
#include <ServoEasing.hpp>
#include <AccelStepper.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ------------------ CONFIGURACIÓN DE RED ------------------
const char *ssid_primary = "CAMBIA_WIFI_SSID";
const char *password_primary = "CAMBIA_WIFI_PASS";
const char *ssid_secondary = "CAMBIA_WIFI_SSID2";
const char *password_secondary = "CAMBIA_WIFI_PASS2";

WiFiMulti wifiMulti;
WebServer httpServer(80);

// Buffer de logs para la consola web
String webSerialBuffer = "";
char pendingWebChar = 0; // Guarda el carácter enviado desde la web

// ------------------ CONFIGURACIÓN DE PINES ESP32 ------------------
// Lado Derecho del Rover (W4 FR / W5 MR / W6 BR) - GPIO remapeados tras giro 180
#define motorW4_IN1 17
#define motorW4_IN2 16
#define motorW5_IN1 4
#define motorW5_IN2 2
#define motorW6_IN1 18
#define motorW6_IN2 5
#define PIN_SERVO_W4 21
#define PIN_SERVO_W6 19
#define PIN_BATTERY 34

// Lado Izquierdo del Rover (W1 FL / W2 ML / W3 BL) - GPIO remapeados tras giro 180
#define motorW1_IN1 27
#define motorW1_IN2 14
#define motorW2_IN1 26
#define motorW2_IN2 25
#define motorW3_IN1 33
#define motorW3_IN2 32
#define PIN_SERVO_W1 13
#define PIN_SERVO_W3 12
#define PIN_SERVO_CAM_TILT 15
#define PIN_STEPPER_STEP 23
#define PIN_STEPPER_DIR 22

// ------------------ CALIBRACIÓN DE SERVOS ------------------
#define OFFSET_SERVO_W1_US 1430
#define OFFSET_SERVO_W3_US 1408
#define OFFSET_SERVO_W4_US 1523
#define OFFSET_SERVO_W6_US 1522
#define OFFSET_SERVO_CAM_TILT_US 1472

// ------------------ INSTANCIAS ------------------
ServoEasing servoW1;
ServoEasing servoW3;
ServoEasing servoW4;
ServoEasing servoW6;
ServoEasing servoCamTilt;

AccelStepper camPanStepper(1, PIN_STEPPER_STEP, PIN_STEPPER_DIR);

// Prototipos
void printMenu();
void testServosDireccion();
void testServoCameraTilt();
void testStepperCameraPan();
void testMotoresTraccion();
void testLecturaBateria();
void testCompleto();
void detenerTodosLosMotores();

// ------------------ SISTEMA DE LOGS HÍBRIDO (SERIE + WEB) ------------------
void logPrint(String msg) {
  Serial.print(msg);
  webSerialBuffer += msg;
  // Limitar el buffer web a 4000 caracteres para evitar desbordar memoria
  if (webSerialBuffer.length() > 4000) {
    webSerialBuffer = webSerialBuffer.substring(webSerialBuffer.length() - 4000);
  }
}

void logPrintln(String msg) {
  logPrint(msg + "\n");
}

void logPrintln() {
  logPrint("\n");
}

// ------------------ PAGINA WEB DEL MONITOR SERIE ------------------
const char* htmlConsolePage = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Rover Web Serial Console</title>
  <link href="https://fonts.googleapis.com/css2?family=Roboto+Mono:wght@400;500;700&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-color: #080808;
      --text-color: #00ff66;
      --panel-border: rgba(0, 255, 102, 0.2);
      --font-family: 'Roboto Mono', monospace;
    }
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      background-color: var(--bg-color);
      color: var(--text-color);
      font-family: var(--font-family);
      height: 100vh;
      display: flex;
      flex-direction: column;
      padding: 15px;
      overflow: hidden;
    }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding-bottom: 10px;
      border-bottom: 1px solid var(--panel-border);
      margin-bottom: 15px;
      font-size: 0.95rem;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      background-color: var(--text-color);
      border-radius: 50%;
      display: inline-block;
      margin-right: 8px;
      box-shadow: 0 0 8px var(--text-color);
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0% { opacity: 0.5; }
      50% { opacity: 1; }
      100% { opacity: 0.5; }
    }
    #terminal {
      flex: 1;
      background-color: #050505;
      border: 1px solid var(--panel-border);
      padding: 15px;
      overflow-y: auto;
      font-size: 0.85rem;
      line-height: 1.4;
      white-space: pre-wrap;
      border-radius: 4px;
      box-shadow: inset 0 0 20px rgba(0,0,0,0.8);
      margin-bottom: 15px;
    }
    .input-area {
      display: flex;
      gap: 10px;
      margin-bottom: 15px;
    }
    input[type="text"] {
      flex: 1;
      background-color: #000;
      border: 1px solid var(--panel-border);
      color: var(--text-color);
      padding: 10px 15px;
      font-family: var(--font-family);
      font-size: 0.9rem;
      border-radius: 4px;
      outline: none;
    }
    input[type="text"]:focus {
      border-color: #fff;
      box-shadow: 0 0 10px rgba(255,255,255,0.1);
    }
    button {
      background-color: transparent;
      border: 1px solid var(--text-color);
      color: var(--text-color);
      padding: 10px 20px;
      font-family: var(--font-family);
      font-size: 0.9rem;
      cursor: pointer;
      border-radius: 4px;
      transition: all 0.3s ease;
    }
    button:hover {
      background-color: var(--text-color);
      color: #000;
      box-shadow: 0 0 15px var(--text-color);
    }
    .keypad {
      display: grid;
      grid-template-cols: repeat(auto-fit, minmax(130px, 1fr));
      gap: 10px;
      padding: 5px 0;
    }
    .keypad-btn {
      font-size: 0.75rem;
      padding: 8px 12px;
      text-transform: uppercase;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <header>
    <div>
      <span class="status-dot"></span>
      <span>ROVER PERSEVERANCE: CONSOLA DE DIAGNÓSTICO WIFI</span>
    </div>
    <div id="ip-display">CONECTADO</div>
  </header>

  <div id="terminal">Cargando consola...</div>

  <div class="input-area">
    <input type="text" id="cmdInput" placeholder="Escribe un comando/opción (1-6) y presiona Enter..." autofocus>
    <button onclick="sendConsoleInput()">Enviar</button>
  </div>

  <div class="keypad">
    <button class="keypad-btn" onclick="quickSend('1')">1. Servos Dir</button>
    <button class="keypad-btn" onclick="quickSend('2')">2. Servo Tilt</button>
    <button class="keypad-btn" onclick="quickSend('3')">3. Stepper Pan</button>
    <button class="keypad-btn" onclick="quickSend('4')">4. Motores Trac</button>
    <button class="keypad-btn" onclick="quickSend('5')">5. Leer Bat</button>
    <button class="keypad-btn" onclick="quickSend('6')">6. Test Completo</button>
  </div>

  <script>
    const terminal = document.getElementById('terminal');
    const cmdInput = document.getElementById('cmdInput');

    // Polling de logs cada 250ms
    setInterval(fetchLogs, 250);

    function fetchLogs() {
      fetch('/read-logs')
        .then(response => response.text())
        .then(text => {
          if (text.length > 0) {
            // Si el terminal muestra el texto inicial de carga, lo limpiamos
            if (terminal.innerText.startsWith("Cargando consola...")) {
              terminal.innerHTML = "";
            }
            terminal.innerHTML += text;
            terminal.scrollTop = terminal.scrollHeight; // Auto-scroll
          }
        });
    }

    function sendConsoleInput() {
      const val = cmdInput.value.trim();
      if (val.length > 0) {
        // Enviar solo el primer carácter a la cola del menú del ESP32
        const charToSend = val.charAt(0);
        sendChar(charToSend);
        cmdInput.value = "";
      }
    }

    function quickSend(char) {
      sendChar(char);
    }

    function sendChar(char) {
      fetch('/send-input?char=' + encodeURIComponent(char), { method: 'POST' });
    }

    cmdInput.addEventListener('keypress', function (e) {
      if (e.key === 'Enter') {
        sendConsoleInput();
      }
    });
  </script>
</body>
</html>
)rawhtml";

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);
  
  // Liberar pines JTAG del ESP32 para GPIOs estándar
  gpio_reset_pin(GPIO_NUM_12);
  gpio_reset_pin(GPIO_NUM_13);
  gpio_reset_pin(GPIO_NUM_14);
  gpio_reset_pin(GPIO_NUM_15);

  // Desactivar resistencias internas heredadas para evitar ruidos y giros espontáneos
  gpio_pullup_dis(GPIO_NUM_12);
  gpio_pullup_dis(GPIO_NUM_13);
  gpio_pullup_dis(GPIO_NUM_14);
  gpio_pullup_dis(GPIO_NUM_15);
  gpio_pulldown_dis(GPIO_NUM_12);
  gpio_pulldown_dis(GPIO_NUM_13);
  gpio_pulldown_dis(GPIO_NUM_14);
  gpio_pulldown_dis(GPIO_NUM_15);

  // Inicializar pines de motor como salidas bajas
  detenerTodosLosMotores();

  // Configurar stepper
  camPanStepper.setMaxSpeed(1000);
  camPanStepper.setAcceleration(500);

  // Inicializar WiFi
  Serial.println("Inicializando WiFi Multi...");
  wifiMulti.addAP(ssid_primary, password_primary);
  wifiMulti.addAP(ssid_secondary, password_secondary);

  Serial.println("Conectando a la red...");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado.");
  Serial.print("SSID: "); Serial.println(WiFi.SSID());
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // Registrar dominio local MDNS (perseverance-rover-esp32.local)
  if (MDNS.begin("perseverance-rover-esp32")) {
    Serial.println("MDNS activado. Puedes usar: http://perseverance-rover-esp32.local");
  }

  // ------------------ RUTAS DEL SERVIDOR WEB ------------------
  httpServer.on("/serial", HTTP_GET, []() {
    httpServer.send(200, "text/html", htmlConsolePage);
  });

  httpServer.on("/read-logs", HTTP_GET, []() {
    httpServer.send(200, "text/plain", webSerialBuffer);
    webSerialBuffer = ""; // Limpiar el buffer de envío tras su lectura
  });

  httpServer.on("/send-input", HTTP_POST, []() {
    if (httpServer.hasArg("char")) {
      String str = httpServer.arg("char");
      if (str.length() > 0) {
        pendingWebChar = str.charAt(0);
      }
    }
    httpServer.send(200, "text/plain", "OK");
  });

  // Redirección simple al serial
  httpServer.on("/", HTTP_GET, []() {
    httpServer.sendHeader("Location", "/serial");
    httpServer.send(302, "text/plain", "");
  });

  httpServer.begin();
  Serial.println("Servidor Web de Consola iniciado.");

  printMenu();
}

void loop() {
  httpServer.handleClient();
  
  char option = 0;

  // Leer desde el puerto Serie físico
  if (Serial.available() > 0) {
    option = Serial.read();
    // Vaciar buffer
    while(Serial.available() > 0) Serial.read();
  } 
  // O leer desde la Consola Web
  else if (pendingWebChar != 0) {
    option = pendingWebChar;
    pendingWebChar = 0; // Consumir carácter
  }

  if (option != 0) {
    switch (option) {
      case '1':
        testServosDireccion();
        break;
      case '2':
        testServoCameraTilt();
        break;
      case '3':
        testStepperCameraPan();
        break;
      case '4':
        testMotoresTraccion();
        break;
      case '5':
        testLecturaBateria();
        break;
      case '6':
        testCompleto();
        break;
      case '\n':
      case '\r':
        // Ignorar saltos de línea
        break;
      default:
        logPrintln("\n[ERROR] Opción no válida.");
        printMenu();
        break;
    }
  }
}

// ------------------ MENÚ DE CONTROL HÍBRIDO ------------------
void printMenu() {
  logPrintln("\n==========================================================");
  logPrintln("       DIAGNÓSTICO INTERACTIVO DE HARDWARE - MARS ROVER    ");
  logPrintln("==========================================================");
  logPrintln("  1. Probar Servos de Dirección (W1, W3, W4, W6)");
  logPrintln("  2. Probar Servo de Elevación de Cámara (TILT - Pin 15)");
  logPrintln("  3. Probar Stepper de Paneo de Cámara (PAN - Pins 22 y 23)");
  logPrintln("  4. Probar Motores de Tracción (W1 a W6)");
  logPrintln("  5. Leer Voltaje de Batería (Pin 34)");
  logPrintln("  6. Ejecutar TEST COMPLETO (Todo consecutivo)");
  logPrintln("==========================================================");
  logPrint("Ingresa una opción (1-6): ");
}

// ------------------ AYUDANTES DE MOTORES ------------------
void setMotorPinPWM(int pin, int val) {
  if (val <= 0) {
    ledcDetach(pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  } else {
    analogWriteFrequency(pin, 200);
    analogWriteResolution(pin, 8);
    analogWrite(pin, val);
  }
}

void detenerTodosLosMotores() {
  setMotorPinPWM(motorW1_IN1, 0);
  setMotorPinPWM(motorW1_IN2, 0);
  setMotorPinPWM(motorW2_IN1, 0);
  setMotorPinPWM(motorW2_IN2, 0);
  setMotorPinPWM(motorW3_IN1, 0);
  setMotorPinPWM(motorW3_IN2, 0);
  setMotorPinPWM(motorW4_IN1, 0);
  setMotorPinPWM(motorW4_IN2, 0);
  setMotorPinPWM(motorW5_IN1, 0);
  setMotorPinPWM(motorW5_IN2, 0);
  setMotorPinPWM(motorW6_IN1, 0);
  setMotorPinPWM(motorW6_IN2, 0);
}

// ------------------ PRUEBA 1: SERVOS DE DIRECCIÓN ------------------
void testServosDireccion() {
  logPrintln("\n--- [INICIO] Test de Servos de Dirección (W1, W3, W4, W6) ---");
  
  // Acoplar servos
  logPrintln("[INFO] Acoplando servos en pines 19, 21, 12, 13...");
  servoW1.attach(PIN_SERVO_W1);
  servoW3.attach(PIN_SERVO_W3);
  servoW4.attach(PIN_SERVO_W4);
  servoW6.attach(PIN_SERVO_W6);

  servoW1.setSpeed(200);
  servoW3.setSpeed(200);
  servoW4.setSpeed(200);
  servoW6.setSpeed(200);

  // Posicionar en neutro calibrado
  logPrintln("[INFO] Centrando todos los servos...");
  servoW1.writeMicroseconds(OFFSET_SERVO_W1_US);
  servoW3.writeMicroseconds(OFFSET_SERVO_W3_US);
  servoW4.writeMicroseconds(OFFSET_SERVO_W4_US);
  servoW6.writeMicroseconds(OFFSET_SERVO_W6_US);
  
  // Procesar servidor web durante la espera para no bloquear la consola
  unsigned long tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  // Sweep W1 (Frontal Izquierdo)
  logPrintln("[TEST] Girando Servo W1 (Frontal Izq)...");
  servoW1.startEaseTo(OFFSET_SERVO_W1_US - 300); // Izquierda
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW1.startEaseTo(OFFSET_SERVO_W1_US + 300); // Derecha
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW1.startEaseTo(OFFSET_SERVO_W1_US); // Retorno
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  // Sweep W3 (Trasero Izquierdo)
  logPrintln("[TEST] Girando Servo W3 (Trasero Izq)...");
  servoW3.startEaseTo(OFFSET_SERVO_W3_US - 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW3.startEaseTo(OFFSET_SERVO_W3_US + 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW3.startEaseTo(OFFSET_SERVO_W3_US);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  // Sweep W4 (Frontal Derecho)
  logPrintln("[TEST] Girando Servo W4 (Frontal Der)...");
  servoW4.startEaseTo(OFFSET_SERVO_W4_US - 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW4.startEaseTo(OFFSET_SERVO_W4_US + 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW4.startEaseTo(OFFSET_SERVO_W4_US);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  // Sweep W6 (Trasero Derecho)
  logPrintln("[TEST] Girando Servo W6 (Trasero Der)...");
  servoW6.startEaseTo(OFFSET_SERVO_W6_US - 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW6.startEaseTo(OFFSET_SERVO_W6_US + 300);
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  
  servoW6.startEaseTo(OFFSET_SERVO_W6_US);
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  // Desacoplar servos por seguridad/consumo
  logPrintln("[INFO] Desacoplando servos.");
  servoW1.detach();
  servoW3.detach();
  servoW4.detach();
  servoW6.detach();

  logPrintln("--- [FIN] Test de Servos de Dirección ---");
  printMenu();
}

// ------------------ PRUEBA 2: SERVO TILT CÁMARA ------------------
void testServoCameraTilt() {
  logPrintln("\n--- [INICIO] Test de Servo Elevación Cámara (TILT - Pin 15) ---");
  
  logPrintln("[INFO] Acoplando servo Cam Tilt...");
  servoCamTilt.attach(PIN_SERVO_CAM_TILT);
  servoCamTilt.setSpeed(150);

  // Ir al centro (90°)
  logPrintln("[INFO] Centrando TILT a 90° (1472 us)...");
  servoCamTilt.writeMicroseconds(OFFSET_SERVO_CAM_TILT_US);
  unsigned long tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  // Sweep a 35° (Límite superior físico)
  logPrintln("[TEST] Elevando a 35°...");
  int targetUs35 = OFFSET_SERVO_CAM_TILT_US + (35 - 90) * 10.311;
  servoCamTilt.startEaseTo(targetUs35);
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  // Sweep a 165° (Límite inferior físico)
  logPrintln("[TEST] Descendiendo a 165°...");
  int targetUs165 = OFFSET_SERVO_CAM_TILT_US + (165 - 90) * 10.311;
  servoCamTilt.startEaseTo(targetUs165);
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  // Volver al centro
  logPrintln("[INFO] Retornando al centro (90°)...");
  servoCamTilt.startEaseTo(OFFSET_SERVO_CAM_TILT_US);
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  logPrintln("[INFO] Desacoplando servo Cam Tilt.");
  servoCamTilt.detach();

  logPrintln("--- [FIN] Test de Servo Elevación Cámara ---");
  printMenu();
}

// ------------------ PRUEBA 3: STEPPER PAN CÁMARA ------------------
void testStepperCameraPan() {
  logPrintln("\n--- [INICIO] Test de Stepper Paneo Cámara (PAN - Pins 22 y 23) ---");
  
  logPrintln("[TEST] Girando Stepper 400 pasos en sentido horario...");
  camPanStepper.move(400);
  while (camPanStepper.distanceToGo() != 0) {
    camPanStepper.run();
    httpServer.handleClient();
  }
  unsigned long tStart = millis();
  while(millis() - tStart < 500) { httpServer.handleClient(); delay(10); }

  logPrintln("[TEST] Girando Stepper 400 pasos en sentido antihorario...");
  camPanStepper.move(-400);
  while (camPanStepper.distanceToGo() != 0) {
    camPanStepper.run();
    httpServer.handleClient();
  }
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  logPrintln("--- [FIN] Test de Stepper Paneo Cámara ---");
  printMenu();
}

// ------------------ PRUEBA 4: MOTORES DE TRACCIÓN ------------------
void testMotoresTraccion() {
  logPrintln("\n--- [INICIO] Test de Motores de Tracción (W1 a W6) ---");
  logPrintln("[WARNING] Asegúrate de que el Rover esté suspendido o tenga espacio libre.");

  int motores_IN1[] = {motorW1_IN1, motorW2_IN1, motorW3_IN1, motorW4_IN1, motorW5_IN1, motorW6_IN1};
  int motores_IN2[] = {motorW1_IN2, motorW2_IN2, motorW3_IN2, motorW4_IN2, motorW5_IN2, motorW6_IN2};
  const char* nombres[] = {"W1 (Frontal Izq)", "W2 (Central Izq)", "W3 (Trasero Izq)", 
                           "W4 (Frontal Der)", "W5 (Central Der)", "W6 (Trasero Der)"};

  for (int i = 0; i < 6; i++) {
    logPrintln();
    logPrint("[TEST] Probando Motor ");
    logPrint(String(i + 1));
    logPrint(": ");
    logPrintln(nombres[i]);

    // Sentido 1: ADELANTE
    logPrintln("  -> Girando sentido ADELANTE (PWM: 180)...");
    int pinActivo = (i >= 3) ? motores_IN2[i] : motores_IN1[i];
    int pinInactivo = (i >= 3) ? motores_IN1[i] : motores_IN2[i];

    setMotorPinPWM(pinActivo, 180);
    setMotorPinPWM(pinInactivo, 0);
    unsigned long tStart = millis();
    while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

    // Detener
    logPrintln("  -> Parada...");
    detenerTodosLosMotores();
    tStart = millis();
    while(millis() - tStart < 800) { httpServer.handleClient(); delay(10); }

    // Sentido 2: ATRÁS
    logPrintln("  -> Girando sentido ATRÁS (PWM: 180)...");
    setMotorPinPWM(pinInactivo, 180);
    setMotorPinPWM(pinActivo, 0);
    tStart = millis();
    while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

    // Detener
    logPrintln("  -> Parada...");
    detenerTodosLosMotores();
    tStart = millis();
    while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }
  }

  logPrintln("\n--- [FIN] Test de Motores de Tracción ---");
  printMenu();
}

// ------------------ PRUEBA 5: LECTURA DE BATERÍA ------------------
void testLecturaBateria() {
  logPrintln("\n--- [INICIO] Test de Lectura de Batería (Pin 34) ---");
  
  unsigned long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogReadMilliVolts(PIN_BATTERY);
    delayMicroseconds(50);
  }
  float avgMV = sum / 50.0;
  float pinVolts = avgMV / 1000.00;
  
  const float FACTOR_CALIBRACION = 4.1;
  float batteryVolts = pinVolts * FACTOR_CALIBRACION;
  
  int pct = map(batteryVolts * 100, 1050, 1260, 0, 100);
  pct = constrain(pct, 0, 100);

  logPrint("[BATERÍA] Voltaje crudo en el Pin: ");
  logPrint(String(pinVolts, 3));
  logPrintln(" V");
  
  logPrint("[BATERÍA] Voltaje compensado del Pack (3S): ");
  logPrint(String(batteryVolts, 2));
  logPrintln(" V");
  
  logPrint("[BATERÍA] Porcentaje aproximado: ");
  logPrint(String(pct));
  logPrintln(" %");

  logPrintln("--- [FIN] Test de Lectura de Batería ---");
  printMenu();
}

// ------------------ PRUEBA 6: TEST COMPLETO ------------------
void testCompleto() {
  logPrintln("\n==========================================================");
  logPrintln("           INICIANDO SECUENCIA COMPLETA DE DIAGNÓSTICO     ");
  logPrintln("==========================================================");
  unsigned long tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  testLecturaBateria();
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  testServosDireccion();
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  testServoCameraTilt();
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  testStepperCameraPan();
  tStart = millis();
  while(millis() - tStart < 1500) { httpServer.handleClient(); delay(10); }

  testMotoresTraccion();
  tStart = millis();
  while(millis() - tStart < 1000) { httpServer.handleClient(); delay(10); }

  logPrintln("\n==========================================================");
  logPrintln("          DIAGNÓSTICO COMPLETO FINALIZADO CON ÉXITO        ");
  logPrintln("==========================================================");
  printMenu();
}

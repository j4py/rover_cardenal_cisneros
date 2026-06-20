/*
 *    Mars Rover - ESP32 Code with Integrated Component Testing App support
 *    Based on original by Dejan, www.HowToMechatronics.com
 *    Adapted for Web Dashboard and interactive physical testing.
 *
 *   Libraries needed for ESP32:
 *   - ESP32Servo: https://github.com/madhephaestus/ESP32Servo
 *   - ServoEasing: https://github.com/ArminJo/ServoEasing
 *   - AccelStepper:
 * http://www.airspayce.com/mikem/arduino/AccelStepper/index.html
 *   - PubSubClient: For MQTT communication
 *   - ArduinoJson: For parsing Dashboard payloads
 */

#include "driver/gpio.h"
#include <AccelStepper.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <ServoEasing.hpp>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>

// ------------------ CONFIGURACIÓN DE RED Y MQTT ------------------
const char *ssid_primary = "CAMBIA_WIFI_SSID";
const char *password_primary = "CAMBIA_WIFI_PASS";
const char *ssid_secondary = "CAMBIA_WIFI_SSID2";
const char *password_secondary = "CAMBIA_WIFI_PASS2";
const char *ssid_tertiary = "CAMBIA_WIFI_SSID3";
const char *password_tertiary = "CAMBIA_WIFI_PASS3";

const char *mqtt_server =
    "CAMBIA_MQTT_HOST";    // Servidor DDNS Cloudflare (Nube Gris)
const int mqtt_port = 1883; // Puerto estándar MQTT
const char *mqtt_user = "CAMBIA_MQTT_USER";
const char *mqtt_password = "CAMBIA_MQTT_PASS";

WiFiClient espClient;
PubSubClient client(espClient);
WiFiMulti wifiMulti;
WebServer httpServer(80);

// ------------------ CONFIGURACIÓN DE PINES ESP32
// Lado Derecho del Rover (W4 FR / W5 MR / W6 BR) - GPIO remapeados tras giro
// 180
#define motorW4_IN1 17
#define motorW4_IN2 16
#define motorW5_IN1 4
#define motorW5_IN2 2
#define motorW6_IN1 18
#define motorW6_IN2 5
#define PIN_SERVO_W4 21
#define PIN_SERVO_W6 19
#define PIN_BATTERY 34

// Lado Izquierdo del Rover (W1 FL / W2 ML / W3 BL) - GPIO remapeados tras giro
// 180
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

// Inversion de gimbal (camara remontada tras giro 180)
#define PAN_DIR -1 // Invierte el sentido del pan de la camara

// ------------------ CALIBRACIÓN DE SERVOS EN MICROSEGUNDOS (PUNTOS 0)
// ------------------
#define OFFSET_SERVO_W1_US 1430       // Calibrado fino
#define OFFSET_SERVO_W3_US 1478       // Calibrado fino
#define OFFSET_SERVO_W4_US 1523       // Calibrado fino
#define OFFSET_SERVO_W6_US 1522       // Calibrado fino
#define OFFSET_SERVO_CAM_TILT_US 1472 // Antiguo 90°

#define SCALE_W3_US_PER_DEG 8.0 // Miuzei 20kg - reducir si W3 sigue girando de mas

// ------------------ INSTANCIAS DE OBJETOS ------------------
ServoEasing servoW1;
ServoEasing servoW3;
ServoEasing servoW4;
ServoEasing servoW6;
ServoEasing servoCamTilt;

AccelStepper camPanStepper(1, PIN_STEPPER_STEP, PIN_STEPPER_DIR);

// ------------------ VARIABLES DE ESTADO ------------------
float joyX = 0;
float joyY = 0;
int ch0 = 1500; // Dirección (1500 = recto)
int ch6 = 1500; // Marcha (1500 = parado, <1500 adelante, >1500 atrás)
int s = 0;      // Velocidad del rover (0 a 100)
int r = 0;
bool spinMode = false; // Radio de giro
float camTilt = 90.0;
int camPan = 0;

float speed1, speed2, speed3 = 0;
float speed1PWM, speed2PWM, speed3PWM = 0;
float thetaInnerFront, thetaInnerBack, thetaOuterFront, thetaOuterBack = 0;

// Geometría del rover en mm
// d2/d3 intercambiados tras giro 180: el eje delantero nuevo es el trasero
// antiguo.
float d1 = 271;
float d2 = 301;
float d3 = 278;
float d4 = 304;

unsigned long lastTelemetryTime = 0;

// VARIABLES PARA EL MODO TEST INTERACTIVO
bool testMode = false;
unsigned long testMotorStopTime = 0;

struct KickstartState {
  int activePin;
  int targetSpeed;
  unsigned long endTime;
};
KickstartState kickstartMotors[6] = {{-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
                                     {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}};

// VARIABLES PARA PREVENIR RUIDOS Y OPTIMIZAR EL CONSUMO
bool motorsRunning = false;
float lastAngleW1 = -1;
float lastAngleW3 = -1;
float lastAngleW4 = -1;
float lastAngleW6 = -1;

void setServoAngle(ServoEasing &servo, float angleInDegrees, int offsetUs,
                   float &lastAngle, float usPerDeg = 10.311) {
  if (abs(angleInDegrees - lastAngle) > 0.05) {
    int targetUs = offsetUs + (angleInDegrees - 90) * usPerDeg;
    servo.startEaseTo(targetUs);
    lastAngle = angleInDegrees;
  }
}

void setMotorPin(int pin, int val) {
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

// ------------------ FUNCIONES DE PRUEBA ------------------
void stopMotors();
void applyKickstart(int dir);
void calculateMotorsSpeed();
void calculateServoAngle();

void runSingleMotor(int id, int dir, int speed, bool stopOthers = true) {
  // Primero detenemos todos los motores por seguridad si se solicita
  if (stopOthers) {
    stopMotors();
  }

  if (dir == 0)
    return;

  Serial.print("[TEST] Girando Motor ");
  Serial.print(id);
  Serial.print(dir == 1 ? " ADELANTE" : " ATRAS");
  Serial.print(" a velocidad ");
  Serial.println(speed);

  int pinActive = -1;
  int pinInactive = -1;

  // Determinar los pines correspondientes según el ID y dirección
  if (id == 1) {
    if (dir == 1) {
      pinActive = motorW1_IN1;
      pinInactive = motorW1_IN2;
    } else {
      pinActive = motorW1_IN2;
      pinInactive = motorW1_IN1;
    }
  } else if (id == 2) {
    if (dir == 1) {
      pinActive = motorW2_IN1;
      pinInactive = motorW2_IN2;
    } else {
      pinActive = motorW2_IN2;
      pinInactive = motorW2_IN1;
    }
  } else if (id == 3) {
    if (dir == 1) {
      pinActive = motorW3_IN1;
      pinInactive = motorW3_IN2;
    } else {
      pinActive = motorW3_IN2;
      pinInactive = motorW3_IN1;
    }
  } else if (id == 4) { // Lógica de sentido invertido para montaje espejo
    if (dir == 1) {
      pinActive = motorW4_IN2;
      pinInactive = motorW4_IN1;
    } else {
      pinActive = motorW4_IN1;
      pinInactive = motorW4_IN2;
    }
  } else if (id == 5) { // Lógica espejo
    if (dir == 1) {
      pinActive = motorW5_IN2;
      pinInactive = motorW5_IN1;
    } else {
      pinActive = motorW5_IN1;
      pinInactive = motorW5_IN2;
    }
  } else if (id == 6) { // Lógica espejo
    if (dir == 1) {
      pinActive = motorW6_IN2;
      pinInactive = motorW6_IN1;
    } else {
      pinActive = motorW6_IN1;
      pinInactive = motorW6_IN2;
    }
  }

  if (pinActive != -1 && pinInactive != -1) {
    // Aplicamos Kickstart no bloqueante a 255 si la velocidad es menor a 220
    if (speed < 220) {
      setMotorPin(pinActive, 255);
      setMotorPin(pinInactive, 0);
      kickstartMotors[id - 1].activePin = pinActive;
      kickstartMotors[id - 1].targetSpeed = speed;
      kickstartMotors[id - 1].endTime = millis() + 200; // 200ms de kickstart
    } else {
      setMotorPin(pinActive, speed);
      setMotorPin(pinInactive, 0);
      kickstartMotors[id - 1].activePin =
          -1; // Desactivar cualquier kickstart previo
    }
  }
}

// ------------------ FUNCIONES MQTT ------------------
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.println("Inicializando WiFi Multi...");

  wifiMulti.addAP(ssid_primary, password_primary);
  wifiMulti.addAP(ssid_secondary, password_secondary);
  wifiMulti.addAP(ssid_tertiary, password_tertiary);

  Serial.println("Conectando a la mejor red disponible...");

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado.");
  Serial.print("SSID actual: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Si llega un comando de movimiento joystick, salimos automáticamente del
  // modo Test
  if (String(topic) == "rover/move") {
    testMode = false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      joyX = doc["x"];
      joyY = doc["y"];
      spinMode = doc["spin"] | false;
      ch0 = 1500 + (joyX * 500);
      s = abs(joyY) * 100;

      if (joyY > 0.1)
        ch6 = 1000; // Adelante
      else if (joyY < -0.1)
        ch6 = 2000; // Atrás
      else
        ch6 = 1500; // Parado
    }
  } else if (String(topic) == "rover/gimbal/yaw") {
    testMode =
        false; // Desactivar modo test al recibir control manual de gimbal
    int relativeSteps = message.toInt();
    if (relativeSteps != 0) {
      camPanStepper.move(PAN_DIR * relativeSteps);
    }
  } else if (String(topic) == "rover/gimbal/pitch") {
    testMode =
        false; // Desactivar modo test al recibir control manual de gimbal
    if (message == "CENTER") {
      camTilt = 90.0;
      servoCamTilt.setSpeed(80); // Velocidad media/suave para volver al centro
      int targetUs = OFFSET_SERVO_CAM_TILT_US; // 90°
      servoCamTilt.startEaseTo(targetUs);
    } else {
      float delta = message.toFloat();
      if (delta != 0.0) {
        camTilt = constrain(camTilt + delta, 35.0, 165.0);
        servoCamTilt.setSpeed(
            120); // Velocidad de seguimiento rápida y responsiva
        int targetUs = OFFSET_SERVO_CAM_TILT_US + (camTilt - 90.0) * 10.311;
        servoCamTilt.startEaseTo(targetUs);
      }
    }
  } else if (String(topic) == "rover/cmd") {
    if (message == "STOP") {
      testMode = false;
      spinMode = false;
      s = 0;
      joyY = 0;
      ch6 = 1500;
      ch0 = 1500;
      stopMotors();
      // Detener el stepper del PAN inmediatamente y limpiar pasos pendientes
      camPanStepper.stop();
      camPanStepper.setCurrentPosition(camPanStepper.currentPosition());
    }
  }
  // NUEVO TOPICO DE TESTEO INDIVIDUAL
  else if (String(topic) == "rover/test") {
    Serial.print("[MQTT TEST] Payload recibido: ");
    Serial.println(message);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (!error) {
      testMode =
          true; // Activamos el modo Test, suspendiendo la navegación automática

      String type = doc["type"];
      if (type == "motor") {
        int id = doc["id"];
        int dir = doc["dir"]; // 1=Adelante, -1=Atrás, 0=Detener
        int speed = doc.containsKey("speed") ? (int)doc["speed"] : 180;
        int duration = doc.containsKey("duration")
                           ? (int)doc["duration"]
                           : 1500; // Duración por defecto: 1.5s
        bool stopOthers =
            doc.containsKey("stopOthers") ? (bool)doc["stopOthers"] : true;

        if (dir == 0) {
          stopMotors();
          testMotorStopTime = 0;
        } else {
          runSingleMotor(id, dir, speed, stopOthers);
          testMotorStopTime =
              millis() +
              duration; // Configuramos temporizador de apagado de seguridad
        }
      } else if (type == "servo") {
        String idStr = doc["id"];
        int targetUs = doc["us"];

        Serial.print("[TEST] Moviendo Servo ");
        Serial.print(idStr);
        Serial.print(" a ");
        Serial.print(targetUs);
        Serial.println(" us (ancho de pulso).");

        if (idStr == "1") {
          servoW1.startEaseTo(targetUs);
        } else if (idStr == "3") {
          servoW3.startEaseTo(targetUs);
        } else if (idStr == "4") {
          servoW4.startEaseTo(targetUs);
        } else if (idStr == "6") {
          servoW6.startEaseTo(targetUs);
        } else if (idStr == "cam") {
          if (!servoCamTilt.attached()) {
            int currentUs = servoCamTilt.getCurrentMicroseconds();
            servoCamTilt.attach(PIN_SERVO_CAM_TILT, currentUs, 544, 2400);
          }
          servoCamTilt.setSpeed(200); // Reset a velocidad rápida de test
          servoCamTilt.startEaseTo(targetUs);
        }
      } else if (type == "stepper") {
        // Modo 1: Posición absoluta con "target" (para coreografías precisas)
        if (doc.containsKey("target")) {
          int target = doc["target"];
          Serial.print("[TEST] Stepper moveTo absoluto: ");
          Serial.println(target);
          camPanStepper.moveTo(target);
        }
        // Modo 2: Resetear posición de referencia ("home": true)
        else if (doc.containsKey("home") && (bool)doc["home"]) {
          Serial.println(
              "[TEST] Stepper: Posición actual marcada como HOME (0).");
          camPanStepper.setCurrentPosition(0);
        }
        // Modo 3: Movimiento relativo con "steps" y "dir" (control manual)
        else {
          int steps = doc["steps"];
          int dir = doc["dir"]; // 1 = horario, -1 = antihorario

          Serial.print("[TEST] Girando Stepper ");
          Serial.print(steps);
          Serial.println(" pasos.");

          camPanStepper.move(PAN_DIR * steps * dir);
        }
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect("ESP32Rover", mqtt_user, mqtt_password)) {
      Serial.println("conectado");
      client.subscribe("rover/move");
      client.subscribe("rover/gimbal/yaw");
      client.subscribe("rover/gimbal/pitch");
      client.subscribe("rover/cmd");
      client.subscribe(
          "rover/test"); // Nos suscribimos al nuevo tópico de testeo
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" reintentando en 5 segundos");
      delay(5000);
    }
  }
}

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);

  // Liberar pines JTAG (GPIO 12, 13, 14, 15) para su uso como GPIOs estándar
  gpio_reset_pin(GPIO_NUM_12);
  gpio_reset_pin(GPIO_NUM_13);
  gpio_reset_pin(GPIO_NUM_14);
  gpio_reset_pin(GPIO_NUM_15);

  // Desactivar resistencias internas de pull-up y pull-down heredadas por
  // JTAG/boot-strapping Esto elimina voltajes fantasma, ruidos y giros
  // espontáneos no deseados en la rueda 6 (pines 14 y 15)
  gpio_pullup_dis(GPIO_NUM_12);
  gpio_pullup_dis(GPIO_NUM_13);
  gpio_pullup_dis(GPIO_NUM_14);
  gpio_pullup_dis(GPIO_NUM_15);
  gpio_pulldown_dis(GPIO_NUM_12);
  gpio_pulldown_dis(GPIO_NUM_13);
  gpio_pulldown_dis(GPIO_NUM_14);
  gpio_pulldown_dis(GPIO_NUM_15);

  // Inicializar pines de motor como salidas digitales en LOW (consumen 0
  // canales LEDC)
  setMotorPin(motorW1_IN1, 0);
  setMotorPin(motorW1_IN2, 0);
  setMotorPin(motorW2_IN1, 0);
  setMotorPin(motorW2_IN2, 0);
  setMotorPin(motorW3_IN1, 0);
  setMotorPin(motorW3_IN2, 0);
  setMotorPin(motorW4_IN1, 0);
  setMotorPin(motorW4_IN2, 0);
  setMotorPin(motorW5_IN1, 0);
  setMotorPin(motorW5_IN2, 0);
  setMotorPin(motorW6_IN1, 0);
  setMotorPin(motorW6_IN2, 0);

  // Configurar timers PWM para los Servos en ESP32
  // Comentado para evitar conflictos de hardware con analogWrite en ESP32
  // Core 3.x (WDT Crash) ESP32PWM::allocateTimer(0);
  // ESP32PWM::allocateTimer(1);
  // ESP32PWM::allocateTimer(2);
  // ESP32PWM::allocateTimer(3);

  servoW1.attach(PIN_SERVO_W1);
  servoW3.attach(PIN_SERVO_W3);
  servoW4.attach(PIN_SERVO_W4);
  servoW6.attach(PIN_SERVO_W6);
  servoCamTilt.attach(PIN_SERVO_CAM_TILT);

  servoW1.writeMicroseconds(OFFSET_SERVO_W1_US);
  servoW3.writeMicroseconds(OFFSET_SERVO_W3_US);
  servoW4.writeMicroseconds(OFFSET_SERVO_W4_US);
  servoW6.writeMicroseconds(OFFSET_SERVO_W6_US);
  servoCamTilt.writeMicroseconds(OFFSET_SERVO_CAM_TILT_US);

  servoW1.setSpeed(550);
  servoW3.setSpeed(550);
  servoW4.setSpeed(550);
  servoW6.setSpeed(550);
  servoCamTilt.setSpeed(200);

  camPanStepper.setMaxSpeed(1000);
  camPanStepper.setAcceleration(
      500); // Rampa de aceleración para pruebas suaves

  setup_wifi();

  // Configurar e iniciar Arduino OTA
  ArduinoOTA.setHostname("Perseverance-Rover-ESP32");
  ArduinoOTA.onStart([]() {
    String type =
        (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Inicio de actualizacion OTA (" + type + ")...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nActualizacion OTA completada con exito. Reiniciando...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Fallo de autenticacion");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Fallo al iniciar");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Fallo de conexion");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Fallo al recibir");
    else if (error == OTA_END_ERROR)
      Serial.println("Fallo al finalizar");
  });
  ArduinoOTA.begin();

  // Configurar e iniciar Web OTA
  httpServer.on("/update", HTTP_GET, []() {
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Rover OTA Update</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600&family=Roboto+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-base: #0a0a0a;
      --bg-panel: #111111;
      --bg-panel-hover: #161616;
      --border-color: rgba(255, 255, 255, 0.05);
      --border-highlight: rgba(255, 255, 255, 0.15);
      --border-radius: 3px;
      --text-primary: #e0e0e0;
      --text-secondary: #7e7e7e;
      --text-accent: #fcfcfc;
      --accent-green: #00ff88;
      --accent-blue: #0088ff;
      --shadow-soft: 0 4px 20px rgba(0, 0, 0, 0.5);
      --shadow-inset: inset 0 1px 0 rgba(255, 255, 255, 0.05);
      --ease-anime: cubic-bezier(0.25, 1, 0.5, 1);
    }
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Inter', sans-serif;
      background-color: var(--bg-base);
      color: var(--text-primary);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      overflow-x: hidden;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }
    body::before {
      content: '';
      position: fixed;
      top: 0; left: 0; width: 100vw; height: 100vh;
      background: radial-gradient(circle 800px at 50% 50%, rgba(255, 255, 255, 0.02), transparent 40%);
      pointer-events: none;
      z-index: 0;
    }
    .app-wrapper {
      position: relative;
      z-index: 1;
      width: 100%;
      max-width: 440px;
      padding: 1px;
      background: var(--border-color);
      border: 1px solid var(--border-color);
      border-radius: var(--border-radius);
      box-shadow: var(--shadow-soft);
      margin: 1rem;
    }
    .module {
      background-color: var(--bg-panel);
      padding: 2.5rem 2rem;
      box-shadow: var(--shadow-inset);
      transition: background-color 0.4s var(--ease-anime);
      text-align: center;
    }
    h1 {
      color: var(--text-accent);
      font-size: 1.75rem;
      font-weight: 500;
      letter-spacing: -0.02em;
      margin-bottom: 0.5rem;
    }
    p {
      color: var(--text-secondary);
      font-size: 0.9rem;
      margin-bottom: 2rem;
    }
    .font-mono {
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .dropzone {
      border: 1px dashed var(--border-color);
      border-radius: var(--border-radius);
      padding: 2.5rem 1.5rem;
      background: rgba(255, 255, 255, 0.01);
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      display: flex;
      flex-direction: column;
      align-items: center;
      position: relative;
      margin-bottom: 1.5rem;
    }
    .dropzone:hover, .dropzone.dragover {
      border-color: var(--accent-green);
      background: rgba(0, 255, 136, 0.02);
    }
    .dropzone svg {
      width: 48px;
      height: 48px;
      color: var(--text-secondary);
      margin-bottom: 1rem;
      transition: color 0.4s var(--ease-anime), transform 0.4s var(--ease-anime);
    }
    .dropzone:hover svg, .dropzone.dragover svg {
      color: var(--accent-green);
      transform: scale(1.1);
    }
    .dropzone span {
      font-size: 0.9rem;
      color: var(--text-primary);
      font-weight: 500;
    }
    .dropzone p {
      font-size: 0.8rem;
      color: var(--text-secondary);
      margin: 0.25rem 0 0 0;
    }
    #fileInput {
      display: none;
    }
    .file-info {
      margin-bottom: 1.5rem;
      display: none;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      color: var(--accent-blue);
      background: rgba(0, 136, 255, 0.05);
      padding: 0.75rem;
      border-radius: var(--border-radius);
      border: 1px solid rgba(0, 136, 255, 0.15);
      word-break: break-all;
      text-align: left;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-primary);
      padding: 0.75rem 1.5rem;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.8rem;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      position: relative;
      overflow: hidden;
      text-decoration: none;
      border-radius: var(--border-radius);
      width: 100%;
    }
    .btn::before {
      content: '';
      position: absolute;
      top: 0; left: 0; width: 100%; height: 100%;
      background: var(--text-accent);
      transform: scaleX(0);
      transform-origin: right;
      transition: transform 0.4s var(--ease-anime);
      z-index: -1;
    }
    .btn:hover {
      color: var(--bg-base);
      border-color: var(--text-accent);
      transform: scale(1.02);
    }
    .btn:hover::before {
      transform: scaleX(1);
      transform-origin: left;
    }
    .btn:disabled {
      opacity: 0.3;
      cursor: not-allowed;
      transform: none;
    }
    .btn:disabled::before {
      display: none;
    }
  </style>
</head>
<body>
  <div class="app-wrapper">
    <div class="module">
      <h1>Actualizar Firmware</h1>
      <p class="font-mono">Rover Perseverance</p>
      <form id="uploadForm" method="POST" action="/update" enctype="multipart/form-data">
        <div id="dropzone" class="dropzone">
          <svg fill="none" stroke="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12"></path>
          </svg>
          <span id="droptext">Arrastra tu archivo .bin aquí</span>
          <p>o haz clic para explorar en tu PC</p>
          <input id="fileInput" type="file" name="update" accept=".bin">
        </div>
        <div id="fileInfo" class="file-info"></div>
        <input id="submitBtn" type="submit" class="btn" value="Subir y Actualizar" disabled>
      </form>
    </div>
  </div>
  <script>
    const dropzone = document.getElementById('dropzone');
    const fileInput = document.getElementById('fileInput');
    const fileInfo = document.getElementById('fileInfo');
    const submitBtn = document.getElementById('submitBtn');
    const droptext = document.getElementById('droptext');

    dropzone.addEventListener('click', () => fileInput.click());

    dropzone.addEventListener('dragover', (e) => {
      e.preventDefault();
      dropzone.classList.add('dragover');
    });

    ['dragleave', 'dragend'].forEach(type => {
      dropzone.addEventListener(type, () => {
        dropzone.classList.remove('dragover');
      });
    });

    dropzone.addEventListener('drop', (e) => {
      e.preventDefault();
      dropzone.classList.remove('dragover');
      if (e.dataTransfer.files.length) {
        fileInput.files = e.dataTransfer.files;
        handleFileChange();
      }
    });

    fileInput.addEventListener('change', handleFileChange);

    function handleFileChange() {
      if (fileInput.files.length) {
        const file = fileInput.files[0];
        if (file.name.endsWith('.bin')) {
          fileInfo.textContent = 'Archivo: ' + file.name + ' (' + (file.size/1024/1024).toFixed(2) + ' MB)';
          fileInfo.style.display = 'block';
          submitBtn.disabled = false;
          droptext.textContent = '¡Archivo seleccionado!';
        } else {
          fileInfo.textContent = 'Error: Solo se admiten archivos .bin';
          fileInfo.style.display = 'block';
          submitBtn.disabled = true;
          droptext.textContent = 'Arrastra tu archivo .bin aquí';
        }
      }
    }
  </script>
</body>
</html>
)rawhtml");
  });
  httpServer.on(
      "/update", HTTP_POST,
      []() {
        httpServer.sendHeader("Connection", "close");
        httpServer.send(200, "text/html", R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Actualización Exitosa</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600&family=Roboto+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-base: #0a0a0a;
      --bg-panel: #111111;
      --bg-panel-hover: #161616;
      --border-color: rgba(255, 255, 255, 0.05);
      --border-radius: 3px;
      --text-primary: #e0e0e0;
      --text-secondary: #7e7e7e;
      --text-accent: #fcfcfc;
      --accent-green: #00ff88;
      --shadow-soft: 0 4px 20px rgba(0, 0, 0, 0.5);
      --shadow-inset: inset 0 1px 0 rgba(255, 255, 255, 0.05);
      --ease-anime: cubic-bezier(0.25, 1, 0.5, 1);
    }
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: 'Inter', sans-serif;
      background-color: var(--bg-base);
      color: var(--text-primary);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      overflow-x: hidden;
      line-height: 1.5;
      -webkit-font-smoothing: antialiased;
    }
    body::before {
      content: '';
      position: fixed;
      top: 0; left: 0; width: 100vw; height: 100vh;
      background: radial-gradient(circle 800px at 50% 50%, rgba(255, 255, 255, 0.02), transparent 40%);
      pointer-events: none;
      z-index: 0;
    }
    .app-wrapper {
      position: relative;
      z-index: 1;
      width: 100%;
      max-width: 440px;
      padding: 1px;
      background: var(--border-color);
      border: 1px solid var(--border-color);
      border-radius: var(--border-radius);
      box-shadow: var(--shadow-soft);
      margin: 1rem;
    }
    .module {
      background-color: var(--bg-panel);
      padding: 2.5rem 2rem;
      box-shadow: var(--shadow-inset);
      transition: background-color 0.4s var(--ease-anime);
      text-align: center;
    }
    h1 {
      color: var(--accent-green);
      font-size: 1.75rem;
      font-weight: 500;
      letter-spacing: -0.02em;
      margin-bottom: 1rem;
      text-shadow: 0 0 15px rgba(0, 255, 136, 0.2);
    }
    p {
      color: var(--text-secondary);
      font-size: 0.95rem;
      margin-bottom: 2rem;
      line-height: 1.6;
    }
    .font-mono {
      font-family: 'Roboto Mono', monospace;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-primary);
      padding: 0.75rem 1.5rem;
      font-family: 'Roboto Mono', monospace;
      font-size: 0.8rem;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.4s var(--ease-anime);
      position: relative;
      overflow: hidden;
      text-decoration: none;
      border-radius: var(--border-radius);
      width: 100%;
    }
    .btn::before {
      content: '';
      position: absolute;
      top: 0; left: 0; width: 100%; height: 100%;
      background: var(--text-accent);
      transform: scaleX(0);
      transform-origin: right;
      transition: transform 0.4s var(--ease-anime);
      z-index: -1;
    }
    .btn:hover {
      color: var(--bg-base);
      border-color: var(--text-accent);
      transform: scale(1.02);
    }
    .btn:hover::before {
      transform: scaleX(1);
      transform-origin: left;
    }
  </style>
  <script>
    setTimeout(function() {
      window.location.href = "/update";
    }, 6000);
  </script>
</head>
<body>
  <div class="app-wrapper">
    <div class="module">
      <h1>Actualización Correcta</h1>
      <p>El firmware se ha cargado con éxito.<br>Reiniciando el Rover, por favor espera unos segundos...</p>
      <a href="/update" class="btn">Volver</a>
    </div>
  </div>
</body>
</html>
)rawhtml");
        delay(1000);
        ESP.restart();
      },
      []() {
        HTTPUpload &upload = httpServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin(
                  UPDATE_SIZE_UNKNOWN)) { // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(
                  true)) { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n",
                          upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });
  httpServer.begin();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ------------------ LOOP PRINCIPAL ------------------
void loop() {
  ArduinoOTA.handle();
  httpServer.handleClient();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Publicar telemetría cada 1 segundos
  if (millis() - lastTelemetryTime > 1000) {
    lastTelemetryTime = millis();

    // Multisampling: Leemos 50 veces consecutivas muy rápido en milivoltios
    // usando la calibración de eFuses de Espressif
    unsigned long sum = 0;
    const int numReadings = 50;
    for (int i = 0; i < numReadings; i++) {
      sum += analogReadMilliVolts(PIN_BATTERY);
      delayMicroseconds(50);
    }
    float averageMilliVolts = sum / (float)numReadings;

    // Conversión de milivoltios a voltios en el pin (0-3.3V)
    float pinVoltage = averageMilliVolts / 1000.00;

    // Divisor de voltaje: R1 = 10k, R2 = 3.3k
    // Ajustado por calibración con multímetro usando la referencia linealizada
    const float FACTOR_CALIBRACION = 4.1;
    float rawBatteryVoltage = pinVoltage * FACTOR_CALIBRACION;

    // 1. FILTRO PASO BAJO (EMA) para suavizar las oscilaciones
    static float filteredVoltage = -1.0;
    if (filteredVoltage < 0) {
      filteredVoltage = rawBatteryVoltage; // Inicialización primera lectura
    } else {
      // Coeficiente de suavizado (0.05 = respuesta muy suave y estable)
      filteredVoltage = (0.05 * rawBatteryVoltage) + (0.95 * filteredVoltage);
    }

    // 2. COMPENSACIÓN ACTIVA DE CAÍDA (SAG) POR CONSUMO DE MOTORES
    float sagCompensation = 0.0;
    if (s > 0 && !testMode) {
      // Compensamos la caída interna: 0.0005V por unidad de velocidad s (0.05V
      // a tope) para evitar sobrecompensación
      sagCompensation = s * 0.0004;
    }
    float compensatedVoltage = filteredVoltage + sagCompensation;

    // Usamos el voltaje compensado para calcular el porcentaje de batería
    int batteryPercentage = map(compensatedVoltage * 100, 1050, 1260, 0, 100);
    if (batteryPercentage > 100)
      batteryPercentage = 100;
    if (batteryPercentage < 0)
      batteryPercentage = 0;

    char telemetryPayload[180];
    snprintf(
        telemetryPayload, sizeof(telemetryPayload),
        "{\"battery\":%.2f,\"battery_pct\":%d,\"mode\":\"%s\",\"ip\":\"%s\"}",
        compensatedVoltage, batteryPercentage, testMode ? "TEST" : "MANUAL",
        WiFi.localIP().toString().c_str());
    client.publish("rover/telemetry", telemetryPayload);
    client.publish("rover/status", "{\"gps\":false}");
  }

  // Si estamos en modo Test, ejecutamos los timers de apagado de seguridad del
  // motor
  if (testMode) {
    if (testMotorStopTime > 0 && millis() > testMotorStopTime) {
      stopMotors();
      testMotorStopTime = 0;
      Serial.println(
          "[TEST] Apagado automático de motor por seguridad finalizado.");
    }

    // Procesar kickstarts no bloqueantes para evitar congelar el motor paso a
    // paso
    for (int i = 0; i < 6; i++) {
      if (kickstartMotors[i].activePin != -1 &&
          millis() >= kickstartMotors[i].endTime) {
        setMotorPin(kickstartMotors[i].activePin,
                    kickstartMotors[i].targetSpeed);
        kickstartMotors[i].activePin = -1; // Desactivar una vez completado
      }
    }

    // Ejecutar movimientos del stepper
    camPanStepper.run();
    return; // Saltamos la lógica de conducción automática / joystick mientras
            // probamos
  }

  // --- LÓGICA ORIGINAL DE NAVEGACIÓN AUTOMÁTICA / MANUAL POR JOYSTICK ---

  // Cálculo del radio de giro
  if (ch0 > 1515) {
    r = map(ch0, 1515, 2000, 1400, 600); // giro a la derecha
  } else if (ch0 < 1485) {
    r = map(ch0, 1485, 1000, 1400, 600); // giro a la izquierda
  }

  // Motor Stepper Pan
  camPanStepper.run();

  if (spinMode) {
    int spinPWM = map((int)(abs(joyX) * 100), 0, 100, 140, 210);
    setServoAngle(servoW1, 90, OFFSET_SERVO_W1_US, lastAngleW1);
    setServoAngle(servoW3, 90, OFFSET_SERVO_W3_US, lastAngleW3, SCALE_W3_US_PER_DEG);
    setServoAngle(servoW4, 90, OFFSET_SERVO_W4_US, lastAngleW4);
    setServoAngle(servoW6, 90, OFFSET_SERVO_W6_US, lastAngleW6);
    if (joyX > 0) {
      setMotorPin(motorW1_IN1, spinPWM);
      setMotorPin(motorW1_IN2, 0);
      setMotorPin(motorW2_IN1, spinPWM);
      setMotorPin(motorW2_IN2, 0);
      setMotorPin(motorW3_IN1, spinPWM);
      setMotorPin(motorW3_IN2, 0);
      setMotorPin(motorW4_IN1, spinPWM);
      setMotorPin(motorW4_IN2, 0);
      setMotorPin(motorW5_IN1, spinPWM);
      setMotorPin(motorW5_IN2, 0);
      setMotorPin(motorW6_IN1, spinPWM);
      setMotorPin(motorW6_IN2, 0);
    } else {
      setMotorPin(motorW1_IN1, 0);
      setMotorPin(motorW1_IN2, spinPWM);
      setMotorPin(motorW2_IN1, 0);
      setMotorPin(motorW2_IN2, spinPWM);
      setMotorPin(motorW3_IN1, 0);
      setMotorPin(motorW3_IN2, spinPWM);
      setMotorPin(motorW4_IN1, 0);
      setMotorPin(motorW4_IN2, spinPWM);
      setMotorPin(motorW5_IN1, 0);
      setMotorPin(motorW5_IN2, spinPWM);
      setMotorPin(motorW6_IN1, 0);
      setMotorPin(motorW6_IN2, spinPWM);
    }
    motorsRunning = true;
  } else {
    calculateMotorsSpeed();
    calculateServoAngle();

    // Compensar fricción estática (Stiction) con un pulso de arranque
    // (Kickstart) al iniciar el movimiento
    static bool wasStopped = true;
    bool isCurrentlyMoving = (s > 0 && ch6 != 1500);
    if (isCurrentlyMoving && wasStopped) {
      int dir = (ch6 < 1500) ? 1 : -1;
      applyKickstart(dir);
    }
    wasStopped = !isCurrentlyMoving;

    // Giro a la derecha
    if (ch0 > 1515) {
      setServoAngle(servoW1, 90 + thetaInnerFront, OFFSET_SERVO_W1_US,
                    lastAngleW1);
      setServoAngle(servoW3, 90 - thetaInnerBack, OFFSET_SERVO_W3_US,
                  lastAngleW3, SCALE_W3_US_PER_DEG);
      setServoAngle(servoW4, 90 + thetaOuterFront, OFFSET_SERVO_W4_US,
                    lastAngleW4);
      setServoAngle(servoW6, 90 - thetaOuterBack, OFFSET_SERVO_W6_US,
                    lastAngleW6);

      if (ch6 < 1500 && s > 0) { // Adelante
        setMotorPin(motorW1_IN1, speed1PWM);
        setMotorPin(motorW1_IN2, 0);
        setMotorPin(motorW2_IN1, speed1PWM);
        setMotorPin(motorW2_IN2, 0);
        setMotorPin(motorW3_IN1, speed1PWM);
        setMotorPin(motorW3_IN2, 0);
        setMotorPin(motorW4_IN1, 0);
        setMotorPin(motorW4_IN2, speed2PWM);
        setMotorPin(motorW5_IN1, 0);
        setMotorPin(motorW5_IN2, speed3PWM);
        setMotorPin(motorW6_IN1, 0);
        setMotorPin(motorW6_IN2, speed2PWM);
        motorsRunning = true;
      } else if (ch6 > 1500 && s > 0) { // Atrás
        setMotorPin(motorW1_IN1, 0);
        setMotorPin(motorW1_IN2, speed1PWM);
        setMotorPin(motorW2_IN1, 0);
        setMotorPin(motorW2_IN2, speed1PWM);
        setMotorPin(motorW3_IN1, 0);
        setMotorPin(motorW3_IN2, speed1PWM);
        setMotorPin(motorW4_IN1, speed2PWM);
        setMotorPin(motorW4_IN2, 0);
        setMotorPin(motorW5_IN1, speed3PWM);
        setMotorPin(motorW5_IN2, 0);
        setMotorPin(motorW6_IN1, speed2PWM);
        setMotorPin(motorW6_IN2, 0);
        motorsRunning = true;
      } else {
        if (motorsRunning) {
          stopMotors();
          motorsRunning = false;
        }
      }
    }
    // Giro a la izquierda
    else if (ch0 < 1485) {
      setServoAngle(servoW1, 90 - thetaOuterFront, OFFSET_SERVO_W1_US,
                    lastAngleW1);
      setServoAngle(servoW3, 90 + thetaOuterBack, OFFSET_SERVO_W3_US,
                  lastAngleW3, SCALE_W3_US_PER_DEG);
      setServoAngle(servoW4, 90 - thetaInnerFront, OFFSET_SERVO_W4_US,
                    lastAngleW4);
      setServoAngle(servoW6, 90 + thetaInnerBack, OFFSET_SERVO_W6_US,
                    lastAngleW6);

      if (ch6 < 1500 && s > 0) { // Adelante
        setMotorPin(motorW1_IN1, speed2PWM);
        setMotorPin(motorW1_IN2, 0);
        setMotorPin(motorW2_IN1, speed3PWM);
        setMotorPin(motorW2_IN2, 0);
        setMotorPin(motorW3_IN1, speed2PWM);
        setMotorPin(motorW3_IN2, 0);
        setMotorPin(motorW4_IN1, 0);
        setMotorPin(motorW4_IN2, speed1PWM);
        setMotorPin(motorW5_IN1, 0);
        setMotorPin(motorW5_IN2, speed1PWM);
        setMotorPin(motorW6_IN1, 0);
        setMotorPin(motorW6_IN2, speed1PWM);
        motorsRunning = true;
      } else if (ch6 > 1500 && s > 0) { // Atrás
        setMotorPin(motorW1_IN1, 0);
        setMotorPin(motorW1_IN2, speed2PWM);
        setMotorPin(motorW2_IN1, 0);
        setMotorPin(motorW2_IN2, speed3PWM);
        setMotorPin(motorW3_IN1, 0);
        setMotorPin(motorW3_IN2, speed2PWM);
        setMotorPin(motorW4_IN1, speed1PWM);
        setMotorPin(motorW4_IN2, 0);
        setMotorPin(motorW5_IN1, speed1PWM);
        setMotorPin(motorW5_IN2, 0);
        setMotorPin(motorW6_IN1, speed1PWM);
        setMotorPin(motorW6_IN2, 0);
        motorsRunning = true;
      } else {
        if (motorsRunning) {
          stopMotors();
          motorsRunning = false;
        }
      }
    }
    // Movimiento recto o parado
    else {
      setServoAngle(servoW1, 90, OFFSET_SERVO_W1_US, lastAngleW1);
      setServoAngle(servoW3, 90, OFFSET_SERVO_W3_US, lastAngleW3, SCALE_W3_US_PER_DEG);
      setServoAngle(servoW4, 90, OFFSET_SERVO_W4_US, lastAngleW4);
      setServoAngle(servoW6, 90, OFFSET_SERVO_W6_US, lastAngleW6);

      if (ch6 < 1500 && s > 0) {
        setMotorPin(motorW1_IN1, speed1PWM);
        setMotorPin(motorW1_IN2, 0);
        setMotorPin(motorW2_IN1, speed1PWM);
        setMotorPin(motorW2_IN2, 0);
        setMotorPin(motorW3_IN1, speed1PWM);
        setMotorPin(motorW3_IN2, 0);
        setMotorPin(motorW4_IN1, 0);
        setMotorPin(motorW4_IN2, speed1PWM);
        setMotorPin(motorW5_IN1, 0);
        setMotorPin(motorW5_IN2, speed1PWM);
        setMotorPin(motorW6_IN1, 0);
        setMotorPin(motorW6_IN2, speed1PWM);
        motorsRunning = true;
      } else if (ch6 > 1500 && s > 0) {
        setMotorPin(motorW1_IN1, 0);
        setMotorPin(motorW1_IN2, speed1PWM);
        setMotorPin(motorW2_IN1, 0);
        setMotorPin(motorW2_IN2, speed1PWM);
        setMotorPin(motorW3_IN1, 0);
        setMotorPin(motorW3_IN2, speed1PWM);
        setMotorPin(motorW4_IN1, speed1PWM);
        setMotorPin(motorW4_IN2, 0);
        setMotorPin(motorW5_IN1, speed1PWM);
        setMotorPin(motorW5_IN2, 0);
        setMotorPin(motorW6_IN1, speed1PWM);
        setMotorPin(motorW6_IN2, 0);
        motorsRunning = true;
      } else {
        if (motorsRunning) {
          stopMotors();
          motorsRunning = false;
        }
      }
    }
  }
}

// ------------------ FUNCIONES AUXILIARES ------------------
void applyKickstart(int dir) {
  if (dir == 0)
    return;
  Serial.print("[KICKSTART] Aplicando pulso de arranque de 255 en direccion: ");
  Serial.println(dir == 1 ? "ADELANTE" : "ATRAS");
  if (dir == 1) { // Adelante
    setMotorPin(motorW1_IN1, 255);
    setMotorPin(motorW1_IN2, 0);
    setMotorPin(motorW2_IN1, 255);
    setMotorPin(motorW2_IN2, 0);
    setMotorPin(motorW3_IN1, 255);
    setMotorPin(motorW3_IN2, 0);
    setMotorPin(motorW4_IN1, 0);
    setMotorPin(motorW4_IN2, 255);
    setMotorPin(motorW5_IN1, 0);
    setMotorPin(motorW5_IN2, 255);
    setMotorPin(motorW6_IN1, 0);
    setMotorPin(motorW6_IN2, 255);
  } else { // Atrás
    setMotorPin(motorW1_IN1, 0);
    setMotorPin(motorW1_IN2, 255);
    setMotorPin(motorW2_IN1, 0);
    setMotorPin(motorW2_IN2, 255);
    setMotorPin(motorW3_IN1, 0);
    setMotorPin(motorW3_IN2, 255);
    setMotorPin(motorW4_IN1, 255);
    setMotorPin(motorW4_IN2, 0);
    setMotorPin(motorW5_IN1, 255);
    setMotorPin(motorW5_IN2, 0);
    setMotorPin(motorW6_IN1, 255);
    setMotorPin(motorW6_IN2, 0);
  }
  delay(200); // Pulso de arranque de 200ms para superar la fricción estática
              // del Rover montado
}

void stopMotors() {
  setMotorPin(motorW1_IN1, 0);
  setMotorPin(motorW1_IN2, 0);
  setMotorPin(motorW2_IN1, 0);
  setMotorPin(motorW2_IN2, 0);
  setMotorPin(motorW3_IN1, 0);
  setMotorPin(motorW3_IN2, 0);
  setMotorPin(motorW4_IN1, 0);
  setMotorPin(motorW4_IN2, 0);
  setMotorPin(motorW5_IN1, 0);
  setMotorPin(motorW5_IN2, 0);
  setMotorPin(motorW6_IN1, 0);
  setMotorPin(motorW6_IN2, 0);

  // Limpiar estados de kickstart no bloqueante
  for (int i = 0; i < 6; i++) {
    kickstartMotors[i].activePin = -1;
  }
}

void calculateMotorsSpeed() {
  if (ch0 >= 1485 && ch0 <= 1515) {
    speed1 = speed2 = speed3 = s;
  } else {
    speed1 = s;
    speed2 = s * sqrt(pow(d3, 2) + pow((r - d1), 2)) / (r + d4);
    speed3 = s * (r - d4) / (r + d4);
  }
  if (s == 0) {
    speed1PWM = 0;
    speed2PWM = 0;
    speed3PWM = 0;
  } else {
    speed1PWM = map(round(speed1), 0, 100, 100, 255);
    speed2PWM = map(round(speed2), 0, 100, 100, 255);
    speed3PWM = map(round(speed3), 0, 100, 100, 255);
  }
}

void calculateServoAngle() {
  thetaInnerFront = round((atan((d3 / (r + d1)))) * 180 / PI);
  thetaInnerBack = round((atan((d2 / (r + d1)))) * 180 / PI);
  thetaOuterFront = round((atan((d3 / (r - d1)))) * 180 / PI);
  thetaOuterBack = round((atan((d2 / (r - d1)))) * 180 / PI);
}

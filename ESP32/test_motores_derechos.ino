/*
 *    Mars Rover - Código de Prueba para Motores del Lado Derecho (SOLO MOTORES
 * - USB) Desarrollado para verificar el correcto cableado, sentido de giro y
 * modulación de velocidad (PWM) de los tres motores de tracción derechos.
 */

// --- CONFIGURACIÓN DE PINES (Lado Derecho) ---
#define motorW4_IN1 17 // Rueda 4 - Delantera Derecha
#define motorW4_IN2 16
#define motorW5_IN1 4 // Rueda 5 - Medio Derecha
#define motorW5_IN2 2
#define motorW6_IN1 18 // Rueda 6 - Trasera Derecha
#define motorW6_IN2 5

// Tiempos de prueba (milisegundos)
const int TIEMPO_MOVIMIENTO = 3000;
const int TIEMPO_ESPERA = 1500;

// Función auxiliar para imprimir en el puerto serie (USB)
void printLog(String message, bool newLine = true) {
  if (newLine) {
    Serial.println(message);
  } else {
    Serial.print(message);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=======================================================");
  Serial.println("           MARS ROVER - TEST DE MOTORES DERECHOS           ");
  Serial.println("=======================================================");

  // Inicializar pines con analogWrite a 0 antes de configurar la frecuencia/resolución
  // para que el núcleo ESP32 asigne los canales LEDC correspondientes.
  analogWrite(motorW4_IN1, 0);
  analogWrite(motorW4_IN2, 0);
  analogWrite(motorW5_IN1, 0);
  analogWrite(motorW5_IN2, 0);
  analogWrite(motorW6_IN1, 0);
  analogWrite(motorW6_IN2, 0);

  // Configurar la frecuencia PWM a 200 Hz y resolución de 8 bits para cada pin de motor
  // de forma individual (frecuencia reducida para aumentar el par a bajas velocidades y asegurar conmutación)
  analogWriteFrequency(motorW4_IN1, 200);
  analogWriteFrequency(motorW4_IN2, 200);
  analogWriteFrequency(motorW5_IN1, 200);
  analogWriteFrequency(motorW5_IN2, 200);
  analogWriteFrequency(motorW6_IN1, 200);
  analogWriteFrequency(motorW6_IN2, 200);

  analogWriteResolution(motorW4_IN1, 8);
  analogWriteResolution(motorW4_IN2, 8);
  analogWriteResolution(motorW5_IN1, 8);
  analogWriteResolution(motorW5_IN2, 8);
  analogWriteResolution(motorW6_IN1, 8);
  analogWriteResolution(motorW6_IN2, 8);

  // Configuración de pines como salidas
  pinMode(motorW4_IN1, OUTPUT);
  pinMode(motorW4_IN2, OUTPUT);
  pinMode(motorW5_IN1, OUTPUT);
  pinMode(motorW5_IN2, OUTPUT);
  pinMode(motorW6_IN1, OUTPUT);
  pinMode(motorW6_IN2, OUTPUT);

  detenerTodos();
  Serial.println("Pines inicializados. Comenzando ciclo de pruebas...");
}

void loop() {
  printLog("\n>>> INICIANDO NUEVO CICLO DE PRUEBA <<<");

  // ==========================================
  // PRUEBA 1: RUEDA 4 (DELANTERA DERECHA)
  // ==========================================
  printLog("[PRUEBA 1] Rueda 4 (Delantera Derecha - GPIO 17/16)");

  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW4_IN1, motorW4_IN2, true, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW4_IN1, motorW4_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW4_IN1, motorW4_IN2, true, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW4_IN1, motorW4_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();

  printLog(" Rueda 4 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 2: RUEDA 5 (MEDIO DERECHA)
  // ==========================================
  printLog("[PRUEBA 2] Rueda 5 (Medio Derecha - GPIO 4/2)");

  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW5_IN1, motorW5_IN2, true, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW5_IN1, motorW5_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW5_IN1, motorW5_IN2, true, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW5_IN1, motorW5_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();

  printLog(" Rueda 5 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 3: RUEDA 6 (TRASERA DERECHA)
  // ==========================================
  printLog("[PRUEBA 3] Rueda 6 (Trasera Derecha - GPIO 18/5)");

  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW6_IN1, motorW6_IN2, true, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW6_IN1, motorW6_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW6_IN1, motorW6_IN2, true, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW6_IN1, motorW6_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();

  printLog(" Rueda 6 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 4: TODOS LOS MOTORES DERECHOS A LA VEZ
  // ==========================================
  printLog("[PRUEBA 4] Todos los motores derechos simultáneamente");

  printLog(" -> Todos ADELANTE...");
  probarMotor(motorW4_IN1, motorW4_IN2, true, 255);
  probarMotor(motorW5_IN1, motorW5_IN2, true, 255);
  probarMotor(motorW6_IN1, motorW6_IN2, true, 255);
  delay(TIEMPO_MOVIMIENTO + 1000);

  detenerTodos();
  delay(TIEMPO_ESPERA);

  printLog(" -> Todos ATRÁS...");
  probarMotor(motorW4_IN1, motorW4_IN2, false, 255);
  probarMotor(motorW5_IN1, motorW5_IN2, false, 255);
  probarMotor(motorW6_IN1, motorW6_IN2, false, 255);
  delay(TIEMPO_MOVIMIENTO + 1000);

  detenerTodos();
  printLog(" Prueba de conjunto completada.\n");

  printLog("=======================================================");
  printLog("Ciclo de pruebas terminado. Esperando 10 segundos");
  printLog("antes de reiniciar el test...");
  printLog("=======================================================");
  delay(10000);
}

// --- FUNCIONES CONTROL DE MOTORES ---

void probarMotor(int pinIN1, int pinIN2, bool adelante, int velocidad) {
  if (adelante) {
    analogWrite(pinIN1, 0);         // Usar analogWrite(0) en lugar de digitalWrite(LOW) para anular el PWM en ESP32
    analogWrite(pinIN2, velocidad); // PWM suave y con gran par a 1000Hz
  } else {
    analogWrite(pinIN1, velocidad); // PWM suave y con gran par a 1000Hz
    analogWrite(pinIN2, 0);         // Usar analogWrite(0) en lugar de digitalWrite(LOW) para anular el PWM en ESP32
  }
}

void detenerTodos() {
  // Usar analogWrite(0) en lugar de digitalWrite(LOW) para detener la señal PWM en el ESP32
  analogWrite(motorW4_IN1, 0);
  analogWrite(motorW4_IN2, 0);
  analogWrite(motorW5_IN1, 0);
  analogWrite(motorW5_IN2, 0);
  analogWrite(motorW6_IN1, 0);
  analogWrite(motorW6_IN2, 0);
}

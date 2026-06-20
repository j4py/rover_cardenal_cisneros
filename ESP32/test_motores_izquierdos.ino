/*
 *    Mars Rover - Código de Prueba para Motores del Lado Izquierdo (SOLO MOTORES - USB)
 *    Desarrollado para verificar el correcto cableado, sentido de giro
 *    y modulación de velocidad (PWM) de los tres motores de tracción izquierdos.
 *
 *    Motores controlados:
 *    - Rueda 1 (Delantera Izquierda): GPIO 27 (IN1) y GPIO 14 (IN2)
 *    - Rueda 2 (Medio Izquierda):     GPIO 26 (IN1) y GPIO 25 (IN2)
 *    - Rueda 3 (Trasera Izquierda):   GPIO 33 (IN1) y GPIO 32 (IN2)
 *
 *    Lógica de Giro del Puente H (Lado Izquierdo - Montaje invertido):
 *    - Adelante: IN1 = HIGH o PWM (Velocidad), IN2 = LOW
 *    - Atrás:    IN1 = LOW, IN2 = HIGH o PWM (Velocidad)
 *    - Parado:   IN1 = LOW, IN2 = LOW
 */

// --- CONFIGURACIÓN DE PINES (Lado Izquierdo) ---
#define motorW1_IN1 27  // Rueda 1 - Delantera Izquierda
#define motorW1_IN2 14
#define motorW2_IN1 26  // Rueda 2 - Medio Izquierda
#define motorW2_IN2 25
#define motorW3_IN1 33 // Rueda 3 - Trasera Izquierda
#define motorW3_IN2 32

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
  // Inicialización del puerto serie para telemetría de depuración
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=======================================================");
  Serial.println("           MARS ROVER - TEST DE MOTORES IZQUIERDOS         ");
  Serial.println("=======================================================");

  // Inicializar pines con analogWrite a 0 antes de configurar la frecuencia/resolución
  // para que el núcleo ESP32 asigne los canales LEDC correspondientes.
  analogWrite(motorW1_IN1, 0);
  analogWrite(motorW1_IN2, 0);
  analogWrite(motorW2_IN1, 0);
  analogWrite(motorW2_IN2, 0);
  analogWrite(motorW3_IN1, 0);
  analogWrite(motorW3_IN2, 0);

  // Configurar la frecuencia PWM a 200 Hz y resolución de 8 bits para cada pin de motor
  // de forma individual (frecuencia reducida para aumentar el par a bajas velocidades y asegurar conmutación)
  analogWriteFrequency(motorW1_IN1, 200);
  analogWriteFrequency(motorW1_IN2, 200);
  analogWriteFrequency(motorW2_IN1, 200);
  analogWriteFrequency(motorW2_IN2, 200);
  analogWriteFrequency(motorW3_IN1, 200);
  analogWriteFrequency(motorW3_IN2, 200);

  analogWriteResolution(motorW1_IN1, 8);
  analogWriteResolution(motorW1_IN2, 8);
  analogWriteResolution(motorW2_IN1, 8);
  analogWriteResolution(motorW2_IN2, 8);
  analogWriteResolution(motorW3_IN1, 8);
  analogWriteResolution(motorW3_IN2, 8);

  // Configuración de pines como salidas
  pinMode(motorW1_IN1, OUTPUT);
  pinMode(motorW1_IN2, OUTPUT);
  pinMode(motorW2_IN1, OUTPUT);
  pinMode(motorW2_IN2, OUTPUT);
  pinMode(motorW3_IN1, OUTPUT);
  pinMode(motorW3_IN2, OUTPUT);

  detenerTodos();
  Serial.println("Pines inicializados. Comenzando ciclo de pruebas...");
}

void loop() {
  printLog("\n>>> INICIANDO NUEVO CICLO DE PRUEBA <<<");

  // ==========================================
  // PRUEBA 1: RUEDA 1 (DELANTERA IZQUIERDA)
  // ==========================================
  printLog("[PRUEBA 1] Rueda 1 (Delantera Izquierda - GPIO 27/14)");
  
  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW1_IN1, motorW1_IN2, true, 100); 
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW1_IN1, motorW1_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW1_IN1, motorW1_IN2, true, 200); 
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW1_IN1, motorW1_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  
  printLog(" Rueda 1 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 2: RUEDA 2 (MEDIO IZQUIERDA)
  // ==========================================
  printLog("[PRUEBA 2] Rueda 2 (Medio Izquierda - GPIO 26/25)");
  
  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW2_IN1, motorW2_IN2, true, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW2_IN1, motorW2_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW2_IN1, motorW2_IN2, true, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW2_IN1, motorW2_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  
  printLog(" Rueda 2 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 3: RUEDA 3 (TRASERA IZQUIERDA)
  // ==========================================
  printLog("[PRUEBA 3] Rueda 3 (Trasera Izquierda - GPIO 33/32)");
  
  // 1. Velocidad 100 (Baja)
  printLog(" -> ADELANTE a velocidad 100...");
  probarMotor(motorW3_IN1, motorW3_IN2, true, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 100...");
  probarMotor(motorW3_IN1, motorW3_IN2, false, 100);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);

  // 2. Velocidad 200 (Media)
  printLog(" -> ADELANTE a velocidad 200...");
  probarMotor(motorW3_IN1, motorW3_IN2, true, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> ATRÁS a velocidad 200...");
  probarMotor(motorW3_IN1, motorW3_IN2, false, 200);
  delay(TIEMPO_MOVIMIENTO);
  detenerTodos();
  
  printLog(" Rueda 3 completada.\n");
  delay(TIEMPO_ESPERA * 2);

  // ==========================================
  // PRUEBA 4: TODOS LOS MOTORES IZQUIERDOS A LA VEZ
  // ==========================================
  printLog("[PRUEBA 4] Todos los motores izquierdos simultáneamente");
  
  printLog(" -> Todos ADELANTE...");
  probarMotor(motorW1_IN1, motorW1_IN2, true, 255); 
  probarMotor(motorW2_IN1, motorW2_IN2, true, 255);
  probarMotor(motorW3_IN1, motorW3_IN2, true, 255);
  delay(TIEMPO_MOVIMIENTO + 1000); 
  
  detenerTodos();
  delay(TIEMPO_ESPERA);
  
  printLog(" -> Todos ATRÁS...");
  probarMotor(motorW1_IN1, motorW1_IN2, false, 255);
  probarMotor(motorW2_IN1, motorW2_IN2, false, 255);
  probarMotor(motorW3_IN1, motorW3_IN2, false, 255);
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

/**
 * Activa un motor individual en una dirección y a una velocidad PWM específica.
 * Lógica izquierda:
 * - Adelante (true):  IN1 = PWM, IN2 = LOW
 * - Atrás (false):    IN1 = LOW, IN2 = PWM
 */
void probarMotor(int pinIN1, int pinIN2, bool adelante, int velocidad) {
  if (adelante) {
    analogWrite(pinIN1, velocidad); // PWM en IN1
    analogWrite(pinIN2, 0);         // Usar analogWrite(0) en lugar de digitalWrite(LOW) para anular el PWM en ESP32
  } else {
    analogWrite(pinIN1, 0);         // Usar analogWrite(0) en lugar de digitalWrite(LOW) para anular el PWM en ESP32
    analogWrite(pinIN2, velocidad); // PWM en IN2
  }
}

/**
 * Detiene todos los motores del lado izquierdo escribiendo LOW en ambos pines de entrada.
 */
void detenerTodos() {
  // Usar analogWrite(0) en lugar de digitalWrite(LOW) para detener la señal PWM en el ESP32
  analogWrite(motorW1_IN1, 0);
  analogWrite(motorW1_IN2, 0);
  analogWrite(motorW2_IN1, 0);
  analogWrite(motorW2_IN2, 0);
  analogWrite(motorW3_IN1, 0);
  analogWrite(motorW3_IN2, 0);
}

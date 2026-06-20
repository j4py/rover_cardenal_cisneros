#include <ESP32Servo.h>

#define SERVO_PIN 13

Servo myServo;
int currentPos = 90;

void printHelp() {
  Serial.println("=== Comandos ===");
  Serial.println("  0-180     -> ir a esa posicion (ej: 90)");
  Serial.println("  +N        -> sumar N grados (ej: +10)");
  Serial.println("  -N        -> restar N grados (ej: -10)");
  Serial.println("  sweep     -> barrido completo 0->180->0");
  Serial.println("  center    -> ir al centro (90 grados)");
  Serial.println("  help      -> mostrar esta ayuda");
  Serial.println("================");
}

void moveTo(int pos) {
  pos = constrain(pos, 0, 180);
  myServo.write(pos);
  currentPos = pos;
  Serial.print("-> Posicion: ");
  Serial.print(currentPos);
  Serial.println(" grados");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Servo Test ESP32 ===");
  myServo.attach(SERVO_PIN);
  Serial.print("Servo conectado en GPIO");
  Serial.println(SERVO_PIN);
  moveTo(90);
  Serial.println();
  printHelp();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input == "help") {
      printHelp();
    } else if (input == "center") {
      moveTo(90);
    } else if (input == "sweep") {
      Serial.println("-> Barrido 0->180->0");
      for (int pos = currentPos; pos >= 0; pos -= 2) { myServo.write(pos); delay(20); }
      for (int pos = 0; pos <= 180; pos += 2)         { myServo.write(pos); delay(20); }
      for (int pos = 180; pos >= 90; pos -= 2)        { myServo.write(pos); delay(20); }
      currentPos = 90;
      Serial.println("-> Barrido completado, posicion: 90");
    } else if (input.startsWith("+")) {
      int delta = input.substring(1).toInt();
      moveTo(currentPos + delta);
    } else if (input.startsWith("-")) {
      int delta = input.substring(1).toInt();
      moveTo(currentPos - delta);
    } else {
      int pos = input.toInt();
      if (pos >= 0 && pos <= 180) {
        moveTo(pos);
      } else {
        Serial.println("! Comando no reconocido. Escribe help.");
      }
    }
  }
}

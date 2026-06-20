#include <MechaQMC5883.h>
#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

MechaQMC5883 qmc;

int x_min = 32767,  x_max = -32768;
int y_min = 32767,  y_max = -32768;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  qmc.init();

  Serial.println("\n===== CALIBRACION QMC5883 =====");
  Serial.println("Gira el rover LENTAMENTE 360 grados varias veces.");
  Serial.println("Mantenerlo horizontal y alejado de metales.");
  Serial.println("Cuando los valores min/max se estabilicen, envía");
  Serial.println("cualquier caracter por el monitor serie para terminar.");
  Serial.println("================================\n");
}

void loop() {
  int x, y, z;
  qmc.read(&x, &y, &z);

  if (x < x_min) x_min = x;
  if (x > x_max) x_max = x;
  if (y < y_min) y_min = y;
  if (y > y_max) y_max = y;

  Serial.print("X:");   Serial.print(x);
  Serial.print("  Y:"); Serial.print(y);
  Serial.print("  |  X["); Serial.print(x_min); Serial.print(","); Serial.print(x_max); Serial.print("]");
  Serial.print("  Y["); Serial.print(y_min); Serial.print(","); Serial.print(y_max); Serial.println("]");

  if (Serial.available()) {
    Serial.read();
    float xo = (x_max + x_min) / 2.0f;
    float yo = (y_max + y_min) / 2.0f;

    Serial.println("\n========== RESULTADO ==========");
    Serial.print("x_min="); Serial.print(x_min);
    Serial.print("  x_max="); Serial.println(x_max);
    Serial.print("y_min="); Serial.print(y_min);
    Serial.print("  y_max="); Serial.println(y_max);
    Serial.println();
    Serial.println("Copia estas dos lineas en gps.ino:");
    Serial.print("#define MAG_X_OFFSET ");
    Serial.println(xo, 1);
    Serial.print("#define MAG_Y_OFFSET ");
    Serial.println(yo, 1);
    Serial.println("================================");
    while (true) delay(1000);
  }

  delay(50);
}

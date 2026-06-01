#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("Hello World from PlatformIO!");
}

void loop() {
  Serial.println("Serial Hello World running...");
  delay(1000);
}

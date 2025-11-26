void setup() {
  pinMode(10, OUTPUT); // Buzzer sur D10
}

void loop() {
  digitalWrite(10, HIGH); // Active le buzzer
  delay(500);
  digitalWrite(10, LOW);  // Désactive le buzzer
  delay(500);
}

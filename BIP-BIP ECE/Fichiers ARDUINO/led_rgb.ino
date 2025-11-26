void setup() {
  pinMode(5, OUTPUT); // Rouge
  pinMode(6, OUTPUT); // Vert
  pinMode(9, OUTPUT); // Bleu
}

void loop() {
  analogWrite(5, 255); // Rouge
  analogWrite(6, 0);
  analogWrite(9, 0);
  delay(1000);

  analogWrite(5, 0);
  analogWrite(6, 255); // Vert
  analogWrite(9, 0);
  delay(1000);

  analogWrite(5, 0);
  analogWrite(6, 0);
  analogWrite(9, 255); // Bleu
  delay(1000);
}


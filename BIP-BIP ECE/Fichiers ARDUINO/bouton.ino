void setup() {
  pinMode(2, INPUT_PULLUP); // Bouton sur D2
  Serial.begin(9600);
}

void loop() {
  if (digitalRead(2) == LOW) {
    Serial.println("Bouton appuyé");
    delay(300); // Anti-rebond
  }
}


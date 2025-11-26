#define pinA 3
#define pinB 4

int lastStateA;

void setup() {
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  Serial.begin(9600);
  lastStateA = digitalRead(pinA);
}

void loop() {
  int stateA = digitalRead(pinA);
  if (stateA != lastStateA) {
    if (digitalRead(pinB) != stateA) {
      Serial.println("Rotation droite");
    } else {
      Serial.println("Rotation gauche");
    }
  }
  lastStateA = stateA;
}


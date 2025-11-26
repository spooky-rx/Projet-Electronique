#include <SPI.h>
#include <RF24.h>

RF24 radio(7, 8); // CE sur D7, CSN sur D8
const byte address[6] = "00001";

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
}

void loop() {
  const char text[] = "Hello";
  radio.write(&text, sizeof(text));
  Serial.println("Message envoyé");
  delay(1000);
}


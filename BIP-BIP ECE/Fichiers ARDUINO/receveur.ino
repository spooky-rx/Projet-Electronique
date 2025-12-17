#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===== NRF24 =====
#define NRF_CE  7
#define NRF_CSN 8
RF24 radio(NRF_CE, NRF_CSN);

// IMPORTANT: l'émetteur écrit sur PIPE_TX="BIP1"
// => le récepteur doit OUVRIR un reading pipe sur "BIP1"
const uint8_t PIPE_RX[6] = "BIP1";

// ===== PINS (sujet) =====
#define LED_R 5
#define LED_G 6
#define LED_B 9
#define BUZZER_PIN 10

// SW3 sur A6 (analog only)
#define SW3_A6 A6
#define SW3_THRESHOLD 200

const bool COMMON_ANODE = false;

// ===== LED helpers =====
void pwmWrite(int pin, int val) {
  val = constrain(val, 0, 255);
  if (COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
void ledOff() { pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,0); }

enum MsgPriority { PRIO_LOW=0, PRIO_MED=1, PRIO_HIGH=2 };
void ledSetPriority(uint8_t prio) {
  if (prio == PRIO_LOW) {        // vert
    pwmWrite(LED_R,0); pwmWrite(LED_G,255); pwmWrite(LED_B,0);
  } else if (prio == PRIO_MED) { // bleu
    pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,255);
  } else {                       // rouge
    pwmWrite(LED_R,255); pwmWrite(LED_G,0); pwmWrite(LED_B,0);
  }
}

// ===== Buzzer =====
bool alertActive = false;
unsigned long lastBeepMs = 0;

void buzzerBeepOnce() {
  tone(BUZZER_PIN, 2000);
  delay(120);
  noTone(BUZZER_PIN);
}

void stopAlert() {
  noTone(BUZZER_PIN);
  ledOff();
  alertActive = false;
}

// ===== SW3 read (A6 analog) =====
bool sw3Pressed() {
  int v = analogRead(SW3_A6);
  return (v < SW3_THRESHOLD);
}

// ===== RX reassembly =====
// Format trame 32B :
// [0]=type(0x01) [1]=msgId [2]=seq [3]=total [4]=prio [5]=pseudoLen [6]=payloadLen [7..]=payload
// Payload contient un morceau du flux: pseudo + '\0' + message

#define RX_BUF_MAX 220
char rxStream[RX_BUF_MAX + 1];
uint16_t rxStreamLen = 0;

uint8_t currentMsgId = 0;
uint8_t expectedTotal = 0;
bool receivedSeq[32];     // max 32 trames (suffisant pour ce projet)
uint8_t currentPrio = 0;

void resetReception(uint8_t msgId, uint8_t total, uint8_t prio) {
  currentMsgId = msgId;
  expectedTotal = total;
  currentPrio = prio;

  rxStreamLen = 0;
  rxStream[0] = '\0';

  for (int i = 0; i < 32; i++) receivedSeq[i] = false;
}

bool allFramesReceived() {
  if (expectedTotal == 0) return false;
  for (uint8_t i = 0; i < expectedTotal; i++) {
    if (!receivedSeq[i]) return false;
  }
  return true;
}

void appendPayloadToStream(const uint8_t* payload, uint8_t payloadLen, uint8_t seq) {
  // On suppose que les trames arrivent dans n'importe quel ordre.
  // Pour faire simple (et suffisant en démo), on concatène SEULEMENT si on reçoit dans l'ordre.
  // => Si tu veux un vrai re-order, on peut stocker chaque payload par seq.
  // Ici on fait une version robuste "quasi" : on ne concatène que si seq == nombre déjà reçus contigus.

  // Compter combien on a de trames reçues contiguës depuis 0
  uint8_t contiguous = 0;
  while (contiguous < expectedTotal && receivedSeq[contiguous]) contiguous++;

  // On n'ajoute que si cette trame est la prochaine attendue contiguë
  if (seq != contiguous) return;

  for (uint8_t i = 0; i < payloadLen; i++) {
    if (rxStreamLen >= RX_BUF_MAX) break;
    rxStream[rxStreamLen++] = (char)payload[i];
  }
  rxStream[rxStreamLen] = '\0';
}

void parseAndDisplayMessage() {
  // rxStream = pseudo '\0' message
  const char* ps = rxStream;
  const char* msg = nullptr;

  // trouver '\0'
  for (uint16_t i = 0; i < rxStreamLen; i++) {
    if (rxStream[i] == '\0') {
      msg = &rxStream[i + 1];
      break;
    }
  }
  if (!msg) msg = ""; // fallback

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("From: ");
  display.println(ps);

  display.setCursor(0, 12);
  display.print("Prio: ");
  display.println(currentPrio);

  // afficher le message sur plusieurs lignes
  display.setCursor(0, 24);
  const int charsPerLine = 21;
  int col = 0;

  for (int i = 0; msg[i] != '\0'; i++) {
    display.write(msg[i]);
    col++;
    if (col >= charsPerLine) {
      col = 0;
      display.setCursor(0, display.getCursorY() + 10);
      if (display.getCursorY() > 54) break;
    }
  }

  display.display();
}

bool initOLED() {
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!ok) ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  return ok;
}

void setupRadio() {
  pinMode(10, OUTPUT); // SPI safety (recommandation)
  radio.begin();
  radio.setChannel(42);          // doit matcher l'émetteur (à rendre réglable plus tard)
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads();

  radio.openReadingPipe(1, PIPE_RX);
  radio.startListening();
}

void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  ledOff();
  noTone(BUZZER_PIN);

  Wire.begin();
  Wire.setClock(100000);
  if (!initOLED()) {
    while (1) {}
  }

  setupRadio();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Receiver ready");
  display.println("Listening NRF24...");
  display.display();
}

void loop() {
  // Stop alerte via SW3
  if (alertActive && sw3Pressed()) {
    stopAlert();
  }

  // Beep périodique tant que l'alerte est active (option)
  if (alertActive && millis() - lastBeepMs > 1500) {
    lastBeepMs = millis();
    buzzerBeepOnce();
  }

  // Réception radio
  if (radio.available()) {
    uint8_t frame[32];
    radio.read(&frame, sizeof(frame));

    uint8_t type = frame[0];
    if (type != 0x01) return; // seulement DATA

    uint8_t msgId = frame[1];
    uint8_t seq = frame[2];
    uint8_t total = frame[3];
    uint8_t prio = frame[4];
    uint8_t pseudoLen = frame[5]; (void)pseudoLen; // pas utilisé ici
    uint8_t payloadLen = frame[6];

    if (total == 0 || total > 32) return;
    if (payloadLen > (32 - 7)) payloadLen = 32 - 7;

    // Nouveau message ?
    if (msgId != currentMsgId) {
      resetReception(msgId, total, prio);
    }

    // Marquer trame reçue
    if (seq < 32) receivedSeq[seq] = true;

    // Ajouter payload (simple: seulement si contigu)
    appendPayloadToStream(&frame[7], payloadLen, seq);

    // Si complet => afficher + alerter
    if (allFramesReceived()) {
      parseAndDisplayMessage();

      // Alerte
      ledSetPriority(currentPrio);
      alertActive = true;
      lastBeepMs = 0;
      buzzerBeepOnce();  // bip immédiat à réception
    }
  }

  delay(5);
}



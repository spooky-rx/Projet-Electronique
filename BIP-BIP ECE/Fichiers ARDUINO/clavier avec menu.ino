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
const uint8_t PIPE_TX[6] = "BIP1";  // destinataire

// ===== PINS (conformes sujet) =====
#define ENC_A 3
#define ENC_B 4
#define ENC_BTN 2

#define LED_R 5
#define LED_G 6
#define LED_B 9

#define BUZZER_PIN 10

// SW3 sur A6 (analog only)
#define SW3_A6 A6
// Seuil pour considérer "appuyé" (à ajuster si besoin)
#define SW3_THRESHOLD 200

// ===== Globals encodeur =====
long ticks = 0;
int lastA = HIGH;

// ===== Bouton encodeur (clic court) =====
bool lastEncBtn = HIGH;
bool encShortClick = false;

// ===== SW3 (envoi) =====
bool lastSW3Pressed = false;

// ===== LED helpers =====
const bool COMMON_ANODE = false; // mets true si ta LED est anode commune
void pwmWrite(int pin, int val) {
  val = constrain(val, 0, 255);
  if (COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
void ledOff() { pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,0); }
void ledBlinkGreen(int n) {
  for (int i=0;i<n;i++){ pwmWrite(LED_G,255); delay(120); ledOff(); delay(120); }
}
void ledBlinkRed(int n) {
  for (int i=0;i<n;i++){ pwmWrite(LED_R,255); delay(120); ledOff(); delay(120); }
}

// ===== Buzzer =====
void buzzerBeep2() {
  for (int i=0;i<2;i++){
    tone(BUZZER_PIN, 2000);
    delay(120);
    noTone(BUZZER_PIN);
    delay(120);
  }
}

// ===== Message =====
#define MSG_MAX 120
char msgBuf[MSG_MAX + 1];
uint16_t msgLen = 0;

const char* pseudos[] = {"MATHI", "NATH", "PAUL"};
const int PSEUDO_COUNT = 3;
int pseudoIndex = 0;
const char* pseudo = "MATHI";

uint8_t priority = 2;
uint8_t channel = 42;
static uint8_t nextMsgId = 1;

// ===== Keyboard grid =====
// ' ' = espace | '<' = backspace
const uint8_t ROWS = 4;
const uint8_t COLS = 10;
const char kb[ROWS][COLS] = {
  {'A','B','C','D','E','F','G','H','I','J'},
  {'K','L','M','N','O','P','Q','R','S','T'},
  {'U','V','W','X','Y','Z','0','1','2','3'},
  {'4','5','6','7','8','9',' ','<','?','!'}
};
uint8_t selR = 0, selC = 0;

// ===== UI state =====
enum UiState { STATE_MENU, STATE_KEYBOARD };
UiState uiState = STATE_MENU;

// ===== Utils message =====
void msgAppend(char c) {
  if (msgLen >= MSG_MAX) return;
  msgBuf[msgLen++] = c;
  msgBuf[msgLen] = '\0';
}
void msgBackspace() {
  if (msgLen == 0) return;
  msgLen--;
  msgBuf[msgLen] = '\0';
}
void resetMessage() {
  msgLen = 0;
  msgBuf[0] = '\0';
}

// ===== OLED init auto =====
bool initOLED() {
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!ok) ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  return ok;
}

// ===== Encoder =====
void encoderInit() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  lastA = digitalRead(ENC_A);
  lastEncBtn = digitalRead(ENC_BTN);
}
void encoderUpdate() {
  int A = digitalRead(ENC_A);
  int B = digitalRead(ENC_B);
  if (A != lastA) {
    if (B != A) ticks++;
    else ticks--;
    lastA = A;
  }
}
void encoderButtonUpdate() {
  encShortClick = false;
  bool b = digitalRead(ENC_BTN); // HIGH idle, LOW pressed
  if (lastEncBtn == HIGH && b == LOW) {
    delay(20); // anti-rebond
    if (digitalRead(ENC_BTN) == LOW) {
      while (digitalRead(ENC_BTN) == LOW) delay(1);
      encShortClick = true;
    }
  }
  lastEncBtn = b;
}

// ===== SW3 read (A6 analog) =====
bool sw3Pressed() {
  int v = analogRead(SW3_A6);
  // Pull-up => non appuyé ~1023, appuyé proche de 0 (selon câblage)
  return (v < SW3_THRESHOLD);
}

// ===== NRF24 =====
// Trame 32B : [type][msgId][seq][total][prio][pseudoLen][payloadLen][payload...]
void setupRadio() {
  pinMode(10, OUTPUT); // recommandé sujet si SPI utilisée
  radio.begin();
  radio.setChannel(channel);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(5, 10);
  radio.openWritingPipe(PIPE_TX);
  radio.stopListening();
}

bool sendMessageNRF24() {
  if (msgLen == 0) return false;

  uint8_t msgId = nextMsgId++;
  if (nextMsgId == 0) nextMsgId = 1;

  const uint8_t headerSize = 7;
  const uint8_t maxPayload = 32 - headerSize;

  size_t psLen = strlen(pseudo);
  if (psLen > 15) psLen = 15;

  size_t dataLen = psLen + 1 + msgLen;
  uint8_t total = (dataLen + maxPayload - 1) / maxPayload;
  if (total == 0) total = 1;

  for (uint8_t seq = 0; seq < total; seq++) {
    uint8_t frame[32] = {0};
    frame[0] = 0x01;
    frame[1] = msgId;
    frame[2] = seq;
    frame[3] = total;
    frame[4] = priority;
    frame[5] = (uint8_t)psLen;

    uint8_t payloadLen = 0;
    uint16_t start = (uint16_t)seq * maxPayload;
    uint16_t end = start + maxPayload;
    if (end > dataLen) end = dataLen;

    for (uint16_t i = start; i < end; i++) {
      char c = (i < psLen) ? pseudo[i]
              : (i == psLen) ? '\0'
              : msgBuf[i - psLen - 1];
      frame[headerSize + payloadLen++] = (uint8_t)c;
    }
    frame[6] = payloadLen;

    if (!radio.write(&frame, sizeof(frame))) return false;
    delay(8);
  }
  return true;
}

// ===== Draw =====
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Choisir pseudo:");
  display.println("Tourner + clic");

  int prev = (pseudoIndex + PSEUDO_COUNT - 1) % PSEUDO_COUNT;
  int next = (pseudoIndex + 1) % PSEUDO_COUNT;

  display.setCursor(0, 30);
  display.print("  "); display.println(pseudos[prev]);
  display.print("> "); display.println(pseudos[pseudoIndex]);
  display.print("  "); display.println(pseudos[next]);

  display.display();
}

void drawKeyboard() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Pseudo:");
  display.print(pseudo);
  display.print(" Len:");
  display.print(msgLen);

  display.setCursor(0, 12);
  // affiche les derniers chars si trop long
  int maxShow = 21;
  int start = (msgLen > maxShow) ? (msgLen - maxShow) : 0;
  for (int i = start; i < (int)msgLen; i++) display.write(msgBuf[i]);

  long idx = ticks % (ROWS * COLS);
  if (idx < 0) idx += ROWS * COLS;
  selR = (uint8_t)(idx / COLS);
  selC = (uint8_t)(idx % COLS);

  int y0 = 32;
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      int x = c * 12;
      int y = y0 + r * 8;

      if (r == selR && c == selC) display.drawRect(x, y, 12, 8, SSD1306_WHITE);

      display.setCursor(x + 2, y + 1);
      char k = kb[r][c];
      if (k == ' ') display.print("SP");
      else if (k == '<') display.print("BS");
      else display.write(k);
    }
  }

  display.setCursor(0, 56);
  display.print("SW3=envoi  Enc=edit");

  display.display();
}

// ===== Setup / Loop =====
void setup() {
  msgBuf[0] = '\0';

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  ledOff();

  Wire.begin();
  Wire.setClock(100000);

  if (!initOLED()) {
    while (1) { /* OLED init fail */ }
  }

  encoderInit();
  setupRadio();

  drawMenu();
}

void loop() {
  encoderUpdate();
  encoderButtonUpdate();

  // --- SW3 edge detect (envoi) ---
  bool sw3 = sw3Pressed();
  if (!lastSW3Pressed && sw3 && uiState == STATE_KEYBOARD) {
    bool ok = sendMessageNRF24();
    buzzerBeep2();
    if (ok) ledBlinkGreen(3);
    else ledBlinkRed(3);
  }
  lastSW3Pressed = sw3;

  // --- UI ---
  if (uiState == STATE_MENU) {
    long idx = ticks % PSEUDO_COUNT;
    if (idx < 0) idx += PSEUDO_COUNT;
    pseudoIndex = (int)idx;

    if (encShortClick) {
      pseudo = pseudos[pseudoIndex];
      uiState = STATE_KEYBOARD;
      ticks = 0;
    }

    drawMenu();
    delay(10);
    return;
  }

  // STATE_KEYBOARD
  if (encShortClick) {
    char k = kb[selR][selC];
    if (k == ' ') msgAppend(' ');
    else if (k == '<') msgBackspace();
    else msgAppend(k);
  }

  drawKeyboard();
  delay(10);
}




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

const uint8_t PIPE_TX[6] = "BIP1";
const uint8_t PIPE_RX[6] = "BIP0";

// ===== Encodeur =====
#define ENC_A 3
#define ENC_B 4
#define ENC_SW 2

long ticks = 0;
int lastA = HIGH;

// ===== Bouton =====
bool lastBtn = HIGH;
unsigned long btnDownMs = 0;
bool longPressFired = false;
bool shortClick = false;
bool longClick = false;

// ===== LED RGB =====
#define LED_R 5
#define LED_G 6
#define LED_B 9
const bool COMMON_ANODE = false;

void pwmWrite(int pin, int val) {
  val = constrain(val, 0, 255);
  if (COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
void ledOff() {
  pwmWrite(LED_R, 0);
  pwmWrite(LED_G, 0);
  pwmWrite(LED_B, 0);
}
void ledBlinkGreen(int times) {
  for (int i = 0; i < times; i++) {
    pwmWrite(LED_G, 255);
    delay(120);
    ledOff();
    delay(120);
  }
}
void ledBlinkRed(int times) {
  for (int i = 0; i < times; i++) {
    pwmWrite(LED_R, 255);
    delay(120);
    ledOff();
    delay(120);
  }
}

// ===== BUZZER =====
#define BUZZER_PIN 10

void buzzerBeep2() {
  for (int i = 0; i < 2; i++) {
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

const char* pseudo = "ALICE";
uint8_t priority = 2;
uint8_t radioChannel = 42;
static uint8_t nextMsgId = 1;

// ===== Clavier =====
const uint8_t ROWS = 4;
const uint8_t COLS = 10;

// ' ' = espace | '<' = backspace | '>' = OK / SEND
const char kb[ROWS][COLS] = {
  {'A','B','C','D','E','F','G','H','I','J'},
  {'K','L','M','N','O','P','Q','R','S','T'},
  {'U','V','W','X','Y','Z','0','1','2','3'},
  {'4','5','6','7','8','9',' ','<','>','!'}
};

uint8_t selR = 0;
uint8_t selC = 0;

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

// ===== Encodeur =====
void encoderInit() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  lastA = digitalRead(ENC_A);
}
void encoderUpdate() {
  int A = digitalRead(ENC_A);
  int B = digitalRead(ENC_B);
  if (A != lastA) {
    if (B != A) ticks++;
    else ticks--;
    lastA = A;
    long idx = ticks % (ROWS * COLS);
    if (idx < 0) idx += ROWS * COLS;
    selR = idx / COLS;
    selC = idx % COLS;
  }
}

// ===== Bouton =====
void buttonUpdate() {
  shortClick = false;
  longClick  = false;
  bool btn = digitalRead(ENC_SW);
  unsigned long now = millis();

  if (lastBtn == HIGH && btn == LOW) {
    btnDownMs = now;
    longPressFired = false;
  }
  if (btn == LOW && !longPressFired && now - btnDownMs > 900) {
    longPressFired = true;
    longClick = true;
  }
  if (lastBtn == LOW && btn == HIGH && !longPressFired) {
    shortClick = true;
  }
  lastBtn = btn;
}

// ===== NRF24 =====
void setupRadio() {
  pinMode(10, OUTPUT); // recommandé sujet
  radio.begin();
  radio.setChannel(radioChannel);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(5, 10);
  radio.openWritingPipe(PIPE_TX);
  radio.stopListening();
}

bool sendMessageNRF24() {
  if (msgLen == 0) return false;

  uint8_t msgId = nextMsgId++;
  const uint8_t headerSize = 7;
  const uint8_t maxPayload = 32 - headerSize;

  size_t psLen = strlen(pseudo);
  size_t dataLen = psLen + 1 + msgLen;
  uint8_t total = (dataLen + maxPayload - 1) / maxPayload;

  for (uint8_t seq = 0; seq < total; seq++) {
    uint8_t frame[32] = {0};
    frame[0] = 0x01;
    frame[1] = msgId;
    frame[2] = seq;
    frame[3] = total;
    frame[4] = priority;
    frame[5] = psLen;

    uint8_t payloadLen = 0;
    uint16_t start = seq * maxPayload;
    uint16_t end = min(start + maxPayload, dataLen);

    for (uint16_t i = start; i < end; i++) {
      char c = (i < psLen) ? pseudo[i] :
               (i == psLen) ? '\0' :
               msgBuf[i - psLen - 1];
      frame[headerSize + payloadLen++] = c;
    }
    frame[6] = payloadLen;

    if (!radio.write(&frame, sizeof(frame))) return false;
    delay(8);
  }
  return true;
}

// ===== OLED =====
void drawUI() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("Len:");
  display.print(msgLen);

  display.setCursor(0, 10);
  display.print(msgBuf);

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
      else if (k == '>') display.print("OK");
      else display.write(k);
    }
  }
  display.display();
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  ledOff();

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  encoderInit();
  setupRadio();
  drawUI();
}

void loop() {
  encoderUpdate();
  buttonUpdate();

  if (longClick) resetMessage();

  if (shortClick) {
    char k = kb[selR][selC];
    if (k == ' ') msgAppend(' ');
    else if (k == '<') msgBackspace();
    else if (k == '>') {
      bool ok = sendMessageNRF24();
      buzzerBeep2();
      if (ok) ledBlinkGreen(2);
      else ledBlinkRed(2);
    } else msgAppend(k);
  }

  drawUI();
  delay(10);
}



#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool initOLED() {
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!ok) ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  return ok;
}

// ===== NRF24 =====
#define NRF_CE  7
#define NRF_CSN 8
RF24 radio(NRF_CE, NRF_CSN);
const uint8_t PIPE_RX[6] = "BIP1";
uint8_t RADIO_CH = 42;

// ===== PINS =====
#define ENC_A 3
#define ENC_B 4
#define ENC_BTN 2

#define LED_R 5
#define LED_G 6
#define LED_B 9

#define BUZZER_PIN 10

#define SW3_A6 A6
#define SW3_THRESHOLD 200

const bool COMMON_ANODE = false;

// ===== Paramètres =====
enum LedMode { LEDMODE_PRIORITY = 0, LEDMODE_FIXED = 1 };
enum FixedColor { FC_RED=0, FC_GREEN, FC_BLUE, FC_CYAN, FC_MAGENTA, FC_YELLOW, FC_WHITE };

LedMode ledMode = LEDMODE_PRIORITY;
FixedColor fixedColor = FC_GREEN;
uint8_t ringtoneId = 0; // 0/1/2

// ===== EEPROM (RX) =====
#define EE_RX_MAGIC_ADDR   10
#define EE_RX_MAGIC_VALUE  0xC3
#define EE_RX_VER_ADDR     11
#define EE_RX_VER_VALUE    0x01
#define EE_RX_LEDMODE_ADDR 12
#define EE_RX_COLOR_ADDR   13
#define EE_RX_RING_ADDR    14
#define EE_RX_CRC_ADDR     15

uint8_t crc8_rx(uint8_t lm, uint8_t col, uint8_t ring) {
  return (uint8_t)(lm ^ (col<<1) ^ (ring<<2) ^ 0x33 ^ EE_RX_MAGIC_VALUE ^ EE_RX_VER_VALUE);
}

void eepromRxLoad() {
  uint8_t m = EEPROM.read(EE_RX_MAGIC_ADDR);
  uint8_t v = EEPROM.read(EE_RX_VER_ADDR);
  uint8_t lm = EEPROM.read(EE_RX_LEDMODE_ADDR);
  uint8_t col = EEPROM.read(EE_RX_COLOR_ADDR);
  uint8_t ring = EEPROM.read(EE_RX_RING_ADDR);
  uint8_t c = EEPROM.read(EE_RX_CRC_ADDR);

  if (m != EE_RX_MAGIC_VALUE || v != EE_RX_VER_VALUE) return;
  if (c != crc8_rx(lm, col, ring)) return;

  if (lm <= 1) ledMode = (LedMode)lm;
  if (col <= 6) fixedColor = (FixedColor)col;
  if (ring <= 2) ringtoneId = ring;
}

void eepromRxSave() {
  uint8_t lm = (uint8_t)ledMode;
  uint8_t col = (uint8_t)fixedColor;
  uint8_t ring = (uint8_t)ringtoneId;

  EEPROM.update(EE_RX_MAGIC_ADDR, EE_RX_MAGIC_VALUE);
  EEPROM.update(EE_RX_VER_ADDR, EE_RX_VER_VALUE);
  EEPROM.update(EE_RX_LEDMODE_ADDR, lm);
  EEPROM.update(EE_RX_COLOR_ADDR, col);
  EEPROM.update(EE_RX_RING_ADDR, ring);
  EEPROM.update(EE_RX_CRC_ADDR, crc8_rx(lm, col, ring));
}

// ===== Encodeur =====
long ticks = 0;
int lastA = HIGH;

bool lastBtn = HIGH;
unsigned long btnDownMs = 0;
bool longPressFired = false;
bool shortClick = false;
bool longClick = false;

// ===== Alert state =====
bool alertActive = false;
unsigned long lastPeriodicBeepMs = 0;

// ===== RX reassembly (simple ordre) =====
#define RX_BUF_MAX 220
char rxStream[RX_BUF_MAX + 1];
uint16_t rxStreamLen = 0;

uint8_t currentMsgId = 0;
uint8_t expectedTotal = 0;
uint8_t currentPrio = 0;
uint8_t nextExpectedSeq = 0;

// ===== LED helpers =====
void pwmWrite(int pin, int val) {
  val = constrain(val, 0, 255);
  if (COMMON_ANODE) val = 255 - val;
  analogWrite(pin, val);
}
void ledOff() { pwmWrite(LED_R,0); pwmWrite(LED_G,0); pwmWrite(LED_B,0); }
void ledSetRGB(uint8_t r, uint8_t g, uint8_t b) { pwmWrite(LED_R,r); pwmWrite(LED_G,g); pwmWrite(LED_B,b); }

void ledSetPriority(uint8_t prio) {
  if (prio == 0)      ledSetRGB(0,255,0);
  else if (prio == 1) ledSetRGB(0,0,255);
  else                ledSetRGB(255,0,0);
}

const char* fixedColorName(FixedColor c) {
  switch (c) {
    case FC_RED: return "RED";
    case FC_GREEN: return "GREEN";
    case FC_BLUE: return "BLUE";
    case FC_CYAN: return "CYAN";
    case FC_MAGENTA: return "MAGENTA";
    case FC_YELLOW: return "YELLOW";
    default: return "WHITE";
  }
}

void applyAlertLed(uint8_t prio) {
  if (ledMode == LEDMODE_PRIORITY) {
    ledSetPriority(prio);
  } else {
    switch (fixedColor) {
      case FC_RED:     ledSetRGB(255,0,0); break;
      case FC_GREEN:   ledSetRGB(0,255,0); break;
      case FC_BLUE:    ledSetRGB(0,0,255); break;
      case FC_CYAN:    ledSetRGB(0,255,255); break;
      case FC_MAGENTA: ledSetRGB(255,0,255); break;
      case FC_YELLOW:  ledSetRGB(255,255,0); break;
      default:         ledSetRGB(255,255,255); break;
    }
  }
}

// ===== Buzzer patterns =====
void buzzerStop() { noTone(BUZZER_PIN); }

void ringtone0_once() { tone(BUZZER_PIN, 2000); delay(120); noTone(BUZZER_PIN); }
void ringtone1_once() {
  tone(BUZZER_PIN, 1800); delay(100); noTone(BUZZER_PIN); delay(70);
  tone(BUZZER_PIN, 1800); delay(100); noTone(BUZZER_PIN);
}
void ringtone2_once() {
  for (int f=900; f<=2200; f+=260) { tone(BUZZER_PIN, f); delay(55); }
  noTone(BUZZER_PIN);
}

void playRingtoneOnce(uint8_t id) {
  if (id == 0) ringtone0_once();
  else if (id == 1) ringtone1_once();
  else ringtone2_once();
}

// ===== SW3 =====
bool sw3Pressed() {
  int v = analogRead(SW3_A6);
  return (v < SW3_THRESHOLD);
}

// ===== RX reassembly =====
void resetReception(uint8_t msgId, uint8_t total, uint8_t prio) {
  currentMsgId = msgId;
  expectedTotal = total;
  currentPrio = prio;
  rxStreamLen = 0;
  rxStream[0] = '\0';
  nextExpectedSeq = 0;
}

void appendPayloadIfInOrder(const uint8_t* payload, uint8_t payloadLen, uint8_t seq) {
  if (seq != nextExpectedSeq) return;
  for (uint8_t i=0;i<payloadLen;i++){
    if (rxStreamLen >= RX_BUF_MAX) break;
    rxStream[rxStreamLen++] = (char)payload[i];
  }
  rxStream[rxStreamLen] = '\0';
  nextExpectedSeq++;
}

bool messageComplete() {
  return (expectedTotal > 0 && nextExpectedSeq >= expectedTotal);
}

// ===== UI menu =====
bool menuOpen = false;
int menuIndex = 0; // 0..2

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.println("PARAMETRES (clic)");
  display.println("Hold: quitter");

  display.setCursor(0, 24);
  display.print(menuIndex==0 ? "> " : "  ");
  display.print("LED mode: ");
  display.println(ledMode==LEDMODE_PRIORITY ? "PRIO" : "FIXED");

  display.setCursor(0, 36);
  display.print(menuIndex==1 ? "> " : "  ");
  display.print("LED color: ");
  display.println(fixedColorName(fixedColor));

  display.setCursor(0, 48);
  display.print(menuIndex==2 ? "> " : "  ");
  display.print("Bip: ");
  display.println(ringtoneId);

  display.display();
}

void drawMessageScreen() {
  const char* ps = rxStream;
  const char* msg = "";
  for (uint16_t i=0;i<rxStreamLen;i++){
    if (rxStream[i]=='\0'){ msg = &rxStream[i+1]; break; }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.print("From: ");
  display.println(ps);

  display.setCursor(0,12);
  display.print("Prio:");
  display.print(currentPrio);
  display.print(" LED:");
  display.println(ledMode==LEDMODE_PRIORITY ? "PRIO" : fixedColorName(fixedColor));

  display.setCursor(0,24);
  const int charsPerLine = 21;
  int col=0;
  for (int i=0; msg[i] != '\0'; i++){
    display.write(msg[i]);
    col++;
    if (col>=charsPerLine){
      col=0;
      display.setCursor(0, display.getCursorY()+10);
      if (display.getCursorY()>54) break;
    }
  }

  display.setCursor(0,56);
  display.print("Hold menu | SW3 stop");
  display.display();
}

// ===== Hardware init =====
void setupRadio() {
  pinMode(SS, OUTPUT); // SS = D10 (buzzer) => OUTPUT OK
  radio.begin();
  radio.setChannel(RADIO_CH);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(5, 10);
  radio.setAutoAck(true);
  radio.enableDynamicPayloads();
  radio.openReadingPipe(1, PIPE_RX);
  radio.startListening();
}

void encoderInit() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);
  lastA = digitalRead(ENC_A);
  lastBtn = digitalRead(ENC_BTN);
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

void buttonUpdate() {
  shortClick = false;
  longClick  = false;

  bool b = digitalRead(ENC_BTN);
  unsigned long now = millis();

  if (lastBtn == HIGH && b == LOW) {
    btnDownMs = now;
    longPressFired = false;
  }
  if (b == LOW && !longPressFired && (now - btnDownMs > 900)) {
    longPressFired = true;
    longClick = true;
  }
  if (lastBtn == LOW && b == HIGH && !longPressFired) {
    shortClick = true;
  }
  lastBtn = b;
}

// ===== Alert =====
void stopAlert() {
  alertActive = false;
  buzzerStop();
  ledOff();
}

void startAlert(uint8_t prio) {
  applyAlertLed(prio);
  alertActive = true;
  lastPeriodicBeepMs = 0;
  playRingtoneOnce(ringtoneId);
}

// ===== Setup / Loop =====
void setup() {
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  ledOff();
  buzzerStop();

  // EEPROM: charger réglages
  eepromRxLoad();

  Wire.begin();
  Wire.setClock(100000);
  if (!initOLED()) while(1){}

  encoderInit();
  setupRadio();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Receiver ready");
  display.println("Hold = menu");
  display.display();

  rxStream[0] = '\0';
}

void loop() {
  encoderUpdate();
  buttonUpdate();

  // Hold => toggle menu
  if (longClick) {
    menuOpen = !menuOpen;
    ticks = 0;
  }

  // Stop alerte via SW3
  if (alertActive && sw3Pressed()) stopAlert();

  // Bip périodique
  if (alertActive && millis() - lastPeriodicBeepMs > 1500) {
    lastPeriodicBeepMs = millis();
    playRingtoneOnce(ringtoneId);
  }

  // Menu
  if (menuOpen) {
    int idx = (int)(ticks % 3);
    if (idx < 0) idx += 3;
    menuIndex = idx;

    if (shortClick) {
      if (menuIndex == 0) {
        ledMode = (ledMode == LEDMODE_PRIORITY) ? LEDMODE_FIXED : LEDMODE_PRIORITY;
      } else if (menuIndex == 1) {
        fixedColor = (FixedColor)((fixedColor + 1) % 7);
      } else if (menuIndex == 2) {
        ringtoneId = (ringtoneId + 1) % 3;
      }
      // EEPROM: sauvegarder à chaque changement
      eepromRxSave();
    }

    drawMenu();
    delay(10);
    return;
  }

  // Radio RX
  if (radio.available()) {
    uint8_t frame[32];
    radio.read(&frame, sizeof(frame));

    if (frame[0] != 0x01) return;

    uint8_t msgId = frame[1];
    uint8_t seq   = frame[2];
    uint8_t total = frame[3];
    uint8_t prio  = frame[4];
    uint8_t payloadLen = frame[6];
    if (payloadLen > 25) payloadLen = 25;
    if (total == 0 || total > 32) return;

    if (msgId != currentMsgId) resetReception(msgId, total, prio);

    appendPayloadIfInOrder(&frame[7], payloadLen, seq);

    if (messageComplete()) {
      drawMessageScreen();
      startAlert(currentPrio);
    }
  }

  delay(5);
}

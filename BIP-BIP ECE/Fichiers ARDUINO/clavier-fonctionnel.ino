#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

// ===== Message =====
#define MSG_MAX 120
char msgBuf[MSG_MAX + 1];
uint16_t msgLen = 0;
bool done = false;

// ===== Clavier grille (4 x 10) =====
const uint8_t ROWS = 4;
const uint8_t COLS = 10;

// Convention des touches spéciales:
// ' ' = SP (space)
// '<' = BS (backspace)
// '>' = OK (validate)
const char kb[ROWS][COLS] = {
  {'A','B','C','D','E','F','G','H','I','J'},
  {'K','L','M','N','O','P','Q','R','S','T'},
  {'U','V','W','X','Y','Z','0','1','2','3'},
  {'4','5','6','7','8','9',' ','<','>','!'}
};

uint8_t selR = 0;
uint8_t selC = 0;

// ===== Utils =====
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

    // map ticks -> index 0..(ROWS*COLS-1), avec wrap
    long t = ticks;
    long mod = (long)(ROWS * COLS);
    long idx = t % mod;
    if (idx < 0) idx += mod;

    selR = (uint8_t)(idx / COLS);
    selC = (uint8_t)(idx % COLS);
  }
}

void buttonUpdate() {
  shortClick = false;
  longClick  = false;

  bool btn = digitalRead(ENC_SW); // pull-up => HIGH idle
  unsigned long now = millis();

  if (lastBtn == HIGH && btn == LOW) {  // press
    btnDownMs = now;
    longPressFired = false;
  }

  if (btn == LOW && !longPressFired) {
    if (now - btnDownMs > 900) {
      longPressFired = true;
      longClick = true;
    }
  }

  if (lastBtn == LOW && btn == HIGH) { // release
    if (!longPressFired) shortClick = true;
  }

  lastBtn = btn;
}

void applyKey(char k) {
  if (k == ' ') {          // SP
    msgAppend(' ');
  } else if (k == '<') {   // BS
    msgBackspace();
  } else if (k == '>') {   // OK
    done = true;
  } else {
    msgAppend(k);
  }
}

void drawUI() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ---- Header ----
  display.setCursor(0, 0);
  display.print("Msg ");
  display.print(msgLen);
  display.print("/");
  display.print(MSG_MAX);
  if (done) display.print(" [READY]");

  // ---- Message preview (2 lignes) ----
  const int charsPerLine = 21;
  const int lines = 2;
  const int maxChars = charsPerLine * lines;

  int start = 0;
  if ((int)msgLen > maxChars) start = msgLen - maxChars;

  display.setCursor(0, 10);
  for (int i = 0; i < maxChars; i++) {
    int idx = start + i;
    if (idx >= (int)msgLen) break;
    display.write(msgBuf[idx]);
    if ((i + 1) % charsPerLine == 0) display.setCursor(0, display.getCursorY() + 10);
  }

  // ---- Keyboard grid ----
  int y0 = 32;
  int cellW = 12; // largeur touche
  int cellH = 8;  // hauteur touche

  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      int x = c * cellW;
      int y = y0 + r * cellH;

      bool sel = (r == selR && c == selC);
      if (sel) display.drawRect(x, y, cellW, cellH, SSD1306_WHITE);

      display.setCursor(x + 2, y + 1);
      char k = kb[r][c];
      if (k == ' ') display.print("SP");
      else if (k == '<') display.print("BS");
      else if (k == '>') display.print("OK");
      else display.write(k);
    }
  }

  // ---- Help ----
  display.setCursor(0, 56);
  display.print("Click=add  Hold=reset");

  display.display();
}

void resetAll() {
  done = false;
  msgLen = 0;
  msgBuf[0] = '\0';
}

void setup() {
  msgBuf[0] = '\0';

  Wire.begin();
  Wire.setClock(100000);

  // init OLED auto 0x3C / 0x3D
  bool ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!ok) ok = display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  if (!ok) while (1) {}

  encoderInit();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED Keyboard OK");
  display.println("Rotate + Click");
  display.display();
  delay(700);

  drawUI();
}

void loop() {
  encoderUpdate();
  buttonUpdate();

  if (shortClick) {
    if (!done) applyKey(kb[selR][selC]);
  }

  if (longClick) {
    resetAll();
  }

  drawUI();
  delay(10);
}



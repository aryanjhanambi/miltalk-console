/*
 * MILTALK CONSOLE v0.2 - Rev C ESP32-S3 N16R8
 * ST7789V 240x320, SPI mode 3, rotation 2 (180 degrees).
 * A=GPIO21, B=GPIO47, TEST=GPIO14, RESET=GPIO1.
 * BTN_C and BTN_D below mean the TEST and RESET application buttons.
 * Adventure edition: five activities, supplied picture assets and camera learning.
 */
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <math.h>
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include <WiFi.h>
#include <esp_now.h>
#include "game_assets.h"
#include "logo_asset.h"

#define TFT_CS    40
#define TFT_RST   41
#define TFT_DC    42
#define TFT_MOSI  38
#define TFT_SCLK  39
#define TFT_SPI_HZ 16000000
#define BTN_A 21
#define BTN_B 47
#define BTN_C 14
#define BTN_D 1
#define BUZZER_PIN -1

Adafruit_ST7789 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);

// ----------------------------- colours -----------------------------
#define COL_BG     0x10E5
#define COL_ACCENT 0x4ED8
#define COL_TEXT   0xFFDC
#define COL_MUTE   0xA5B9
#define COL_GOOD   0x8F75
#define COL_BAD    0xFAAC
#define COL_WARN   0xFDCE
#define COL_SEL    0x21A9

// ----------------------------- screen layout ------------------------
class ScreenCanvas : public GFXcanvas16 {
public:
  ScreenCanvas() : GFXcanvas16(240, 320, false) {}
  bool begin() {
    buffer = static_cast<uint16_t*>(heap_caps_malloc(
      240 * 320 * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    buffer_owned = (buffer != nullptr);
    return buffer_owned;
  }
};
ScreenCanvas ui;
bool uiDirty = false;
void presentScreen() {
  if (!uiDirty || !ui.getBuffer()) return;
  tft.startWrite();
  tft.setAddrWindow(0, 0, 240, 320);
  tft.writePixels(ui.getBuffer(), 240 * 320);
  tft.endWrite();
  uiDirty = false;
}
void screenPause(unsigned long ms) {
  presentScreen();
  delay(ms);
}

int W, H;      // screen size in pixels
int S;         // text scale: 1 on small screens, 2 on 240-wide screens
int LH;        // line height in pixels
int LAST_ROW;  // last text row that fits on screen

int rowY(int n) { return n * LH + 4 * S; }

void displayBegin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320, SPI_MODE3);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(2);
  ui.setTextWrap(false);
  if (!ui.begin()) {
    tft.fillScreen(ST77XX_RED);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(12, 100);
    tft.print("Enable OPI PSRAM");
    while (true) delay(1000);
  }
  W = tft.width();
  H = tft.height();
  S = (W >= 200) ? 2 : 1;
  LH = 10 * S;
  LAST_ROW = (H - 4 * S - 8 * S) / LH;
}

// Note: no default arguments, so the Arduino IDE's auto-generated prototypes stay valid.
void txtSz(int x, int y, const char* s, uint16_t c, int size) {
  while (size > 1 && x + (int)strlen(s) * 6 * size > W - 4) --size;
  uiDirty = true;
  ui.setTextSize(size);
  ui.setTextColor(c);
  ui.setCursor(x, y);
  ui.print(s);
}

void txt(int x, int y, const char* s, uint16_t c) { txtSz(x, y, s, c, S); }

void ctxtSz(int y, const char* s, uint16_t c, int size) {
  while (size > 1 && (int)strlen(s) * 6 * size > W - 8) --size;
  int w = strlen(s) * 6 * size;
  txtSz((W - w) / 2, y, s, c, size);
}

void ctxt(int y, const char* s, uint16_t c) { ctxtSz(y, s, c, S); }

void header(const char* title) {
  uiDirty = true;
  ui.fillRoundRect(6, 2, W - 12, LH + 2 * S, 8, COL_ACCENT);
  ctxt(2 * S + 2, title, COL_BG);
}

#include "ui_helpers.h"

// ----------------------------- buttons -----------------------------
const int BTN_PINS[4] = {BTN_A, BTN_B, BTN_C, BTN_D};
bool btnDown[4] = {false, false, false, false};
bool btnEdge[4] = {false, false, false, false};
unsigned long btnT[4] = {0, 0, 0, 0};

void readButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    btnEdge[i] = false;
    bool d = digitalRead(BTN_PINS[i]) == LOW;
    if (d != btnDown[i] && now - btnT[i] > 30) {
      btnT[i] = now;
      btnDown[i] = d;
      if (d) btnEdge[i] = true;
    }
  }
}
bool pressed(int i) { return btnEdge[i]; }
bool held(int i)    { return btnDown[i]; }

void beep(int freq, int ms) {
  if (BUZZER_PIN >= 0) tone(BUZZER_PIN, freq, ms);
}


float frand() { return (esp_random() & 0xFFFF) / 65536.0f; }

// Print text across several lines, breaking at spaces. Returns the y after the last line.
int wrapText(int x, int y, const char* s, uint16_t c) {
  int maxChars = (W - x - 4 * S) / (6 * S);
  char line[48];
  int len = 0;
  const char* p = s;
  while (*p) {
    const char* w = p;
    while (*p && *p != ' ') p++;
    int wl = p - w;
    if (len > 0 && len + 1 + wl > maxChars) {
      line[len] = 0;
      txt(x, y, line, c);
      y += LH;
      len = 0;
    }
    if (len > 0 && len < 46) line[len++] = ' ';
    int n = min(wl, 46 - len);
    memcpy(line + len, w, n);
    len += n;
    while (*p == ' ') p++;
  }
  if (len) { line[len] = 0; txt(x, y, line, c); y += LH; }
  return y;
}

// Hold A + B for 1.5 s. Returns true once when the hold completes.
bool holdForget(unsigned long& t) {
  if (held(0) && held(1)) {
    if (t == 0) t = millis();
    if (millis() - t > 1500) {
      t = 0;
      while (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW) delay(5);
      return true;
    }
  } else {
    t = 0;
  }
  return false;
}

void flashMessage(const char* msg) {
  uiDirty = true;
  ui.fillRect(0, H / 2 - 8 * S, W, 16 * S, COL_ACCENT);
  ctxt(H / 2 - 4 * S, msg, COL_TEXT);
  screenPause(900);
}

// Draw a scrolling list in rows firstRow..lastRow with item sel highlighted.
void drawList(const char* const* items, int count, int sel, int firstRow, int lastRow) {
  uiDirty = true;
  int visible = lastRow - firstRow + 1;
  int top = sel - visible / 2;
  if (top > count - visible) top = count - visible;
  if (top < 0) top = 0;
  ui.fillRect(0, rowY(firstRow) - 2 * S, W, rowY(lastRow + 1) - rowY(firstRow), COL_BG);
  for (int i = 0; i < visible && top + i < count; i++) {
    int idx = top + i;
    int y = rowY(firstRow + i);
    if (idx == sel) ui.fillRect(0, y - 2 * S, W, LH, COL_SEL);
    txt(4 * S, y, items[idx], idx == sel ? COL_TEXT : COL_MUTE);
  }
}

// ============================== MIND READER ==============================
// Rock, paper, scissors against an AI that learns your habits.
// The AI counts which move you tend to play after your last two moves
// (a simple pattern model), predicts your next move, and plays what beats it.
// It commits to its move BEFORE you press, so it cannot cheat.

const char* MOVES[3] = {"ROCK", "PAPER", "SCISSORS"};
int beats(int m) { return (m + 1) % 3; }   // PAPER beats ROCK, SCISSORS beats PAPER, ROCK beats SCISSORS

Preferences prefs;

uint16_t mrC2[3][3][3];   // after your last two moves -> next move counts
uint16_t mrC1[3][3];      // after your last move      -> next move counts
uint16_t mrC0[3];         // overall move counts
int8_t   mrP1 = -1, mrP2 = -1;   // your last move, and the one before
uint32_t mrRounds = 0;           // rounds remembered across power cycles

int mrYou = 0, mrAI = 0, mrDraw = 0;   // scores this session
int mrPred = 0, mrConf = 33, mrAiMove = 0;
int8_t mrHits[10];
int mrHitCount = 0, mrHitPos = 0;
int mrSinceSave = 0;

enum MrState { MR_INTRO, MR_PLAY };
MrState mrState = MR_INTRO;
unsigned long mrHoldStart = 0;

void mrLoad() {
  memset(mrC2, 0, sizeof(mrC2));
  memset(mrC1, 0, sizeof(mrC1));
  memset(mrC0, 0, sizeof(mrC0));
  prefs.begin("mindreader", true);
  if (prefs.getBytesLength("c2") == sizeof(mrC2)) {
    prefs.getBytes("c2", mrC2, sizeof(mrC2));
    prefs.getBytes("c1", mrC1, sizeof(mrC1));
    prefs.getBytes("c0", mrC0, sizeof(mrC0));
    mrP1 = prefs.getChar("p1", -1);
    mrP2 = prefs.getChar("p2", -1);
    mrRounds = prefs.getULong("rounds", 0);
  } else {
    mrP1 = mrP2 = -1;
    mrRounds = 0;
  }
  prefs.end();
}

void mrSave() {
  prefs.begin("mindreader", false);
  prefs.putBytes("c2", mrC2, sizeof(mrC2));
  prefs.putBytes("c1", mrC1, sizeof(mrC1));
  prefs.putBytes("c0", mrC0, sizeof(mrC0));
  prefs.putChar("p1", mrP1);
  prefs.putChar("p2", mrP2);
  prefs.putULong("rounds", mrRounds);
  prefs.end();
  mrSinceSave = 0;
  Serial.printf("Mind Reader saved (%lu rounds)\n", (unsigned long)mrRounds);
}

void mrForget() {
  memset(mrC2, 0, sizeof(mrC2));
  memset(mrC1, 0, sizeof(mrC1));
  memset(mrC0, 0, sizeof(mrC0));
  mrP1 = mrP2 = -1;
  mrRounds = 0;
  mrSave();
  Serial.println("Mind Reader memory cleared");
}

int rowSum(const uint16_t* r) { return r[0] + r[1] + r[2]; }

// Pick the most likely next move from the most specific pattern that has data.
void mrPredict() {
  const uint16_t* row = nullptr;
  if (mrP2 >= 0 && mrP1 >= 0 && rowSum(mrC2[mrP2][mrP1]) >= 2) row = mrC2[mrP2][mrP1];
  else if (mrP1 >= 0 && rowSum(mrC1[mrP1]) >= 2)                row = mrC1[mrP1];
  else if (rowSum(mrC0) > 0)                                     row = mrC0;

  if (!row) {                       // no data yet: guess at random
    mrPred = esp_random() % 3;
    mrConf = 33;
  } else {
    int best = -1, bestCount = -1, ties = 0;
    for (int m = 0; m < 3; m++) {
      if (row[m] > bestCount) { best = m; bestCount = row[m]; ties = 1; }
      else if (row[m] == bestCount && (esp_random() % (++ties)) == 0) best = m;   // random tie-break
    }
    mrPred = best;
    mrConf = (bestCount * 100) / rowSum(row);
  }
  mrAiMove = beats(mrPred);
}

void mrLearn(int m) {
  mrC0[m]++;
  if (mrP1 >= 0) mrC1[mrP1][m]++;
  if (mrP2 >= 0 && mrP1 >= 0) mrC2[mrP2][mrP1][m]++;
  // keep counts from overflowing: halve everything if one gets large
  if (mrC0[m] > 60000) {
    for (int a = 0; a < 3; a++) {
      mrC0[a] /= 2;
      for (int b = 0; b < 3; b++) {
        mrC1[a][b] /= 2;
        for (int c = 0; c < 3; c++) mrC2[a][b][c] /= 2;
      }
    }
  }
  mrP2 = mrP1;
  mrP1 = m;
  mrRounds++;
}

void mrDrawIntro() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("MIND READER");
  txt(4 * S, rowY(1), "Rock Paper Scissors", COL_TEXT);
  txt(4 * S, rowY(2), "against the AI.", COL_TEXT);
  txt(4 * S, rowY(3), "It learns your", COL_TEXT);
  txt(4 * S, rowY(4), "habits as you play.", COL_TEXT);
  txt(4 * S, rowY(5), "Can you beat it?", COL_WARN);
  char buf[24];
  snprintf(buf, sizeof(buf), "Played: %lu rounds", (unsigned long)mrRounds);
  txt(4 * S, rowY(6), buf, COL_MUTE);
  txt(4 * S, rowY(LAST_ROW - 2), "TEST: start", COL_GOOD);
  txt(4 * S, rowY(LAST_ROW - 1), "Hold A+B: forget", COL_MUTE);
  txt(4 * S, rowY(LAST_ROW), "RESET: menu", COL_MUTE);
}

// you = the move you just played, or -1 before the first round
void mrDrawPlay(int you, int outcome, int lastPred, int lastConf, bool lastHit) {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("MIND READER");
  char buf[32];

  snprintf(buf, sizeof(buf), "You %d  AI %d  Draw %d", mrYou, mrAI, mrDraw);
  ctxt(rowY(1), buf, COL_TEXT);

  if (you < 0) {
    ctxt(rowY(3), "Make your move!", COL_WARN);
    ctxt(rowY(4), "The AI has already", COL_MUTE);
    ctxt(rowY(5), "chosen its move.", COL_MUTE);
  } else {
    int aiMove = beats(lastPred);
    snprintf(buf, sizeof(buf), "You: %s", MOVES[you]);
    txt(4 * S, rowY(2), buf, COL_TEXT);
    snprintf(buf, sizeof(buf), "AI:  %s", MOVES[aiMove]);
    txt(4 * S, rowY(3), buf, COL_TEXT);

    const char* res = outcome > 0 ? "YOU WIN" : outcome < 0 ? "AI WINS" : "DRAW";
    uint16_t rc = outcome > 0 ? COL_GOOD : outcome < 0 ? COL_BAD : COL_WARN;
    ctxtSz(rowY(4) + 2 * S, res, rc, 2 * S);

    snprintf(buf, sizeof(buf), "Guessed: %s", MOVES[lastPred]);
    txt(4 * S, rowY(6), buf, lastHit ? COL_BAD : COL_MUTE);
    snprintf(buf, sizeof(buf), "Sure: %d%% %s", lastConf, lastHit ? "RIGHT" : "wrong");
    txt(4 * S, rowY(7), buf, lastHit ? COL_BAD : COL_MUTE);
  }

  // last 10 predictions: red = the AI read you, grey = it missed
  int y = rowY(8);
  txt(4 * S, y, "Read", COL_MUTE);
  int sq = 6 * S, gap = 2 * S, x0 = 4 * S + 30 * S;
  for (int i = 0; i < 10; i++) {
    int x = x0 + i * (sq + gap);
    if (x + sq > W) break;
    if (i < mrHitCount) {
      int idx = (mrHitPos - mrHitCount + i + 10) % 10;
      ui.fillRect(x, y, sq, sq, mrHits[idx] ? COL_BAD : COL_MUTE);
    } else {
      ui.drawRect(x, y, sq, sq, COL_MUTE);
    }
  }

  txt(4 * S, rowY(LAST_ROW - 1), "A:Rock  B:Paper", COL_MUTE);
  txt(4 * S, rowY(LAST_ROW), "TEST:Scissors RESET:Menu", COL_MUTE);
}

void mrStart() {
  mrLoad();
  mrYou = mrAI = mrDraw = 0;
  mrHitCount = mrHitPos = 0;
  mrState = MR_INTRO;
  mrHoldStart = 0;
  mrDrawIntro();
}

bool mrUpdate() {
  if (mrState == MR_INTRO) {
    if (pressed(3)) return false;                       // D: back to menu
    if (pressed(2)) {                                   // C: start
      mrState = MR_PLAY;
      mrPredict();
      mrDrawPlay(-1, 0, 0, 0, false);
      beep(880, 60);
      return true;
    }
    if (held(0) && held(1)) {                           // hold A+B: forget
      if (mrHoldStart == 0) mrHoldStart = millis();
      if (millis() - mrHoldStart > 1500) {
        mrForget();
        ui.fillScreen(COL_BG);
        ctxt(H / 2 - 4 * S, "Memory cleared", COL_WARN);
        screenPause(900);
        mrHoldStart = 0;
        while (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW) delay(5);
        mrDrawIntro();
      }
    } else {
      mrHoldStart = 0;
    }
    return true;
  }

  // MR_PLAY
  if (pressed(3)) { mrSave(); return false; }           // D: save and back to menu

  int you = -1;
  if (pressed(0)) you = 0;
  else if (pressed(1)) you = 1;
  else if (pressed(2)) you = 2;
  if (you < 0) return true;

  int lastPred = mrPred, lastConf = mrConf, ai = mrAiMove;
  bool hit = (you == lastPred);
  mrHits[mrHitPos] = hit ? 1 : 0;
  mrHitPos = (mrHitPos + 1) % 10;
  if (mrHitCount < 10) mrHitCount++;

  int outcome;                                          // +1 you win, -1 AI wins, 0 draw
  if (you == ai)            { outcome = 0;  mrDraw++; }
  else if (ai == beats(you)) { outcome = -1; mrAI++;  }
  else                       { outcome = 1;  mrYou++; }

  Serial.printf("You %s | AI %s | AI predicted %s (%d%%) %s\n",
                MOVES[you], MOVES[ai], MOVES[lastPred], lastConf, hit ? "HIT" : "miss");

  mrLearn(you);
  mrPredict();                                          // AI commits to its next move now
  mrDrawPlay(you, outcome, lastPred, lastConf, hit);
  beep(outcome > 0 ? 1200 : outcome < 0 ? 300 : 600, 80);

  if (++mrSinceSave >= 5) mrSave();
  return true;
}


// ============================== PET TRAINER ==============================
// Reinforcement learning. The pet sees a situation and tries an action.
// You reward it (A) or say no (B). It keeps a score (Q-value) for every
// situation + action pair and slowly picks the actions that earned rewards.
// Early on it often tries random things (exploring); that fades with training.

const char* PET_SIT[4] = {"Ball thrown!", "Dinner time", "You say: SIT", "Doorbell rings"};
const char* PET_ACT[4] = {"FETCH", "EAT", "SIT", "BARK"};

float petQ[4][4];
uint32_t petFeedback = 0;
int petSit = 0, petAct = 0;
bool petExplored = false;
int petMood = 0;                 // 1 happy, -1 sad, 0 neutral
enum PetState { PET_INTRO, PET_PLAY };
PetState petState = PET_INTRO;
unsigned long petHold = 0;

void petLoad() {
  memset(petQ, 0, sizeof(petQ));
  prefs.begin("pet", true);
  if (prefs.getBytesLength("q") == sizeof(petQ)) prefs.getBytes("q", petQ, sizeof(petQ));
  petFeedback = prefs.getULong("fb", 0);
  prefs.end();
}

void petSave() {
  prefs.begin("pet", false);
  prefs.putBytes("q", petQ, sizeof(petQ));
  prefs.putULong("fb", petFeedback);
  prefs.end();
}

float petEpsilon() {                     // chance of trying something random
  float e = 0.5f / (1.0f + petFeedback / 8.0f);
  return e < 0.05f ? 0.05f : e;
}

void petChoose() {
  petSit = esp_random() % 4;
  if (frand() < petEpsilon()) {
    petAct = esp_random() % 4;
    petExplored = true;
  } else {
    int best = 0, ties = 1;
    for (int a = 1; a < 4; a++) {
      if (petQ[petSit][a] > petQ[petSit][best]) { best = a; ties = 1; }
      else if (petQ[petSit][a] == petQ[petSit][best] && (esp_random() % (++ties)) == 0) best = a;
    }
    petAct = best;
    petExplored = false;
  }
  petMood = 0;
}

void petDrawFace(int cx, int cy, int r, int mood) {
  drawBuddyReaction(mood);
}

void petDrawBars() {
  uiDirty = true;
  const int x0 = 80, x1 = 224, cx = 152, half = 70;
  for (int a = 0; a < 4; ++a) {
    int y = 197 + a * 18;
    ui.fillRect(8, y - 1, 224, 18, COL_BG);
    txtSz(12, y + 2, PET_ACT[a], a == petAct ? COL_TEXT : COL_MUTE, 1);
    ui.fillRoundRect(x0, y + 2, x1 - x0, 11, 4, COL_SEL);
    float q = constrain(petQ[petSit][a], -1.0f, 1.0f);
    int len = (int)(q * half);
    if (len > 0) ui.fillRect(cx + 1, y + 3, len, 9, COL_GOOD);
    else if (len < 0) ui.fillRect(cx + len, y + 3, -len, 9, COL_BAD);
    ui.drawFastVLine(cx, y + 1, 13, COL_MUTE);
  }
}

void petDrawPlay() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("BUDDY TRAINER");
  ctxt(32, PET_SIT[petSit], COL_WARN);
  drawBuddyReaction(petMood);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s: %s", BUDDY_NAMES[selectedBuddy], PET_ACT[petAct]);
  ctxt(158, buf, COL_TEXT);
  ctxtSz(181, petExplored ? "Trying something new!" : "Using what you taught me", COL_ACCENT, 1);
  petDrawBars();
  ctxtSz(279, "A: GOOD   B: NO   TEST: SKIP", COL_TEXT, 1);
  ctxtSz(302, "RESET: save + menu", COL_MUTE, 1);
}

void petDrawIntro() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("BUDDY TRAINER");
  ctxt(34, "Teach your buddy!", COL_WARN);
  ui.fillRoundRect(48, 58, 144, 142, 12, COL_TEXT);
  drawSprite(*BUDDY_SPRITES[selectedBuddy], 56, 62, 128, 134);
  ctxtSz(210, "A rewards / B says no", COL_TEXT, 1);
  char buf[32];
  snprintf(buf, sizeof(buf), "%lu lessons learned", (unsigned long)petFeedback);
  ctxtSz(229, buf, COL_ACCENT, 1);
  ctxt(252, "TEST: start", COL_GOOD);
  ctxtSz(279, "Hold A+B: forget lessons", COL_MUTE, 1);
  ctxtSz(302, "RESET: menu", COL_MUTE, 1);
}

void petStart() {
  petLoad();
  petState = PET_INTRO;
  petHold = 0;
  petDrawIntro();
}

bool petUpdate() {
  if (petState == PET_INTRO) {
    if (pressed(3)) return false;
    if (pressed(2)) { petState = PET_PLAY; petChoose(); petDrawPlay(); return true; }
    if (holdForget(petHold)) {
      memset(petQ, 0, sizeof(petQ));
      petFeedback = 0;
      petSave();
      flashMessage("Pet forgot all");
      petDrawIntro();
    }
    return true;
  }

  if (pressed(3)) { petSave(); return false; }

  int reward = 0;
  if (pressed(0)) reward = 1;
  else if (pressed(1)) reward = -1;
  else if (pressed(2)) { petChoose(); petDrawPlay(); return true; }   // skip, no lesson
  if (reward == 0) return true;

  petQ[petSit][petAct] += 0.4f * (reward - petQ[petSit][petAct]);
  petFeedback++;
  petMood = reward;
  Serial.printf("Pet: %s -> %s, reward %d, Q=%.2f\n", PET_SIT[petSit], PET_ACT[petAct], reward, petQ[petSit][petAct]);

  petDrawFace(W / 2, rowY(3), (LH * 8) / 10, petMood);   // show the reaction
  petDrawBars();
  beep(reward > 0 ? 1200 : 300, 80);
  screenPause(800);
  if (petFeedback % 5 == 0) petSave();

  petChoose();
  petDrawPlay();
  return true;
}

// ============================== DETECTIVE ==============================
// A decision tree that grows. The AI asks yes/no questions to guess your
// animal. When it is wrong, you tell it the answer and which question
// separates the two animals, and it adds a new branch to its tree.

const char* ANIMALS[] = {"DOG", "CAT", "FISH", "BIRD", "ELEPHANT", "SNAKE", "FROG", "PENGUIN",
                         "SHARK", "LION", "RABBIT", "HORSE", "OWL", "TURTLE", "MONKEY", "COW"};
const int NUM_ANIMALS = sizeof(ANIMALS) / sizeof(ANIMALS[0]);

const char* QUESTIONS[] = {"Does it live in water?", "Can it fly?", "Is it bigger than a person?",
                           "Does it have fur?", "Do people keep it as a pet?", "Does it have four legs?",
                           "Does it eat meat?", "Does it lay eggs?", "Does it have feathers?",
                           "Does it have a tail?", "Is it awake at night?", "Can it climb trees?"};
const char* QSHORT[] = {"Lives in water", "Can fly", "Bigger than person", "Has fur", "Kept as a pet",
                        "Has four legs", "Eats meat", "Lays eggs", "Has feathers", "Has a tail",
                        "Awake at night", "Climbs trees"};
const int NUM_QUESTIONS = sizeof(QUESTIONS) / sizeof(QUESTIONS[0]);

struct DNode { int8_t q; int8_t animal; uint8_t yes; uint8_t no; };   // q < 0 means leaf
#define D_MAX 64
DNode dTree[D_MAX];
uint8_t dCount = 0;

enum DState { D_INTRO, D_ASK, D_GUESS, D_WIN, D_PICK_ANIMAL, D_PICK_Q, D_PICK_ANS, D_DONE };
DState dState = D_INTRO;
int dNode = 0, dAsked = 0, dSel = 0, dNewAnimal = 0, dNewQ = 0;
int dListRow = 3;
unsigned long dHold = 0;

void dDefaultTree() {
  dTree[0] = {0, -1, 1, 2};   // Does it live in water?
  dTree[1] = {-1, 2, 0, 0};   //   FISH
  dTree[2] = {1, -1, 3, 4};   //   Can it fly?
  dTree[3] = {-1, 3, 0, 0};   //     BIRD
  dTree[4] = {-1, 0, 0, 0};   //     DOG
  dCount = 5;
}

void dLoad() {
  prefs.begin("detective", true);
  uint8_t n = prefs.getUChar("n", 0);
  if (n >= 1 && n <= D_MAX && prefs.getBytesLength("t") == sizeof(DNode) * n) {
    prefs.getBytes("t", dTree, sizeof(DNode) * n);
    dCount = n;
  } else {
    dDefaultTree();
  }
  prefs.end();
}

void dSave() {
  prefs.begin("detective", false);
  prefs.putUChar("n", dCount);
  prefs.putBytes("t", dTree, sizeof(DNode) * dCount);
  prefs.end();
}

int dKnown() {
  int n = 0;
  for (int i = 0; i < dCount; i++) if (dTree[i].q < 0) n++;
  return n;
}

void dDrawIntro() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("DETECTIVE");
  txt(4 * S, rowY(1), "Think of an animal.", COL_WARN);
  txt(4 * S, rowY(2), "I will ask yes/no", COL_TEXT);
  txt(4 * S, rowY(3), "questions to guess.", COL_TEXT);
  txt(4 * S, rowY(4), "If I am wrong,", COL_TEXT);
  txt(4 * S, rowY(5), "teach me!", COL_TEXT);
  char buf[24];
  snprintf(buf, sizeof(buf), "I know %d animals", dKnown());
  txt(4 * S, rowY(6), buf, COL_MUTE);
  txt(4 * S, rowY(LAST_ROW - 2), "TEST: start", COL_GOOD);
  txt(4 * S, rowY(LAST_ROW - 1), "Hold A+B: forget", COL_MUTE);
  txt(4 * S, rowY(LAST_ROW), "RESET: menu", COL_MUTE);
}

void dShowNode() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("DETECTIVE");
  char buf[28];
  if (dTree[dNode].q >= 0) {
    dState = D_ASK;
    snprintf(buf, sizeof(buf), "Question %d", dAsked + 1);
    txt(4 * S, rowY(1), buf, COL_MUTE);
    wrapText(4 * S, rowY(3), QUESTIONS[dTree[dNode].q], COL_TEXT);
  } else {
    dState = D_GUESS;
    ctxt(rowY(2), "Is it a...", COL_MUTE);
    ctxtSz(rowY(4), ANIMALS[dTree[dNode].animal], COL_WARN, 2 * S);
  }
  txt(4 * S, rowY(LAST_ROW), "A:Yes  B:No", COL_MUTE);
}

void dDrawPick(const char* prompt, const char* const* items, int count) {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("TEACH ME");
  int endY = wrapText(4 * S, rowY(1), prompt, COL_WARN);
  dListRow = min(LAST_ROW - 1, max(3, (endY + LH - 1) / LH + 1));
  drawList(items, count, dSel, dListRow, LAST_ROW - 1);
  txt(4 * S, rowY(LAST_ROW), "A/B move TEST pick", COL_MUTE);
}

void dDrawEnd(const char* line1, const char* line2) {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("DETECTIVE");
  ctxt(rowY(3), line1, COL_GOOD);
  ctxt(rowY(5), line2, COL_TEXT);
  txt(4 * S, rowY(LAST_ROW - 1), "TEST: play again", COL_MUTE);
  txt(4 * S, rowY(LAST_ROW), "RESET: menu", COL_MUTE);
}

void dLearn(bool newAnimalIsYes) {
  int leaf = dNode;
  int oldAnimal = dTree[leaf].animal;
  int nNew = dCount, nOld = dCount + 1;
  dTree[nNew] = {-1, (int8_t)dNewAnimal, 0, 0};
  dTree[nOld] = {-1, (int8_t)oldAnimal, 0, 0};
  dTree[leaf].q = dNewQ;
  dTree[leaf].animal = -1;
  dTree[leaf].yes = newAnimalIsYes ? nNew : nOld;
  dTree[leaf].no  = newAnimalIsYes ? nOld : nNew;
  dCount += 2;
  dSave();
  Serial.printf("Detective learned %s (tree size %d)\n", ANIMALS[dNewAnimal], dCount);
}

void dStart() {
  dLoad();
  dState = D_INTRO;
  dHold = 0;
  dDrawIntro();
}

bool dUpdate() {
  if (pressed(3)) return false;   // D: menu from anywhere

  switch (dState) {
    case D_INTRO:
      if (pressed(2)) { dNode = 0; dAsked = 0; dShowNode(); }
      if (holdForget(dHold)) { dDefaultTree(); dSave(); flashMessage("Back to 3 animals"); dDrawIntro(); }
      break;

    case D_ASK:
      if (pressed(0) || pressed(1)) {
        dNode = pressed(0) ? dTree[dNode].yes : dTree[dNode].no;
        dAsked++;
        dShowNode();
      }
      break;

    case D_GUESS:
      if (pressed(0)) {
        dState = D_WIN;
        char buf[24];
        snprintf(buf, sizeof(buf), "in %d questions", dAsked);
        dDrawEnd("I got it!", buf);
      } else if (pressed(1)) {
        if (dCount + 2 > D_MAX) {
          dState = D_DONE;
          dDrawEnd("You win!", "My memory is full");
        } else {
          dState = D_PICK_ANIMAL;
          dSel = 0;
          dDrawPick("You win! What was it?", ANIMALS, NUM_ANIMALS);
        }
      }
      break;

    case D_PICK_ANIMAL:
      if (pressed(0)) { dSel = (dSel + NUM_ANIMALS - 1) % NUM_ANIMALS; drawList(ANIMALS, NUM_ANIMALS, dSel, dListRow, LAST_ROW - 1); }
      if (pressed(1)) { dSel = (dSel + 1) % NUM_ANIMALS;               drawList(ANIMALS, NUM_ANIMALS, dSel, dListRow, LAST_ROW - 1); }
      if (pressed(2)) {
        if (dSel == dTree[dNode].animal) { flashMessage("That was my guess!"); dDrawPick("You win! What was it?", ANIMALS, NUM_ANIMALS); break; }
        dNewAnimal = dSel;
        dState = D_PICK_Q;
        dSel = 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "What is true for a %s but not a %s?", ANIMALS[dNewAnimal], ANIMALS[dTree[dNode].animal]);
        dDrawPick(buf, QSHORT, NUM_QUESTIONS);
      }
      break;

    case D_PICK_Q:
      if (pressed(0)) { dSel = (dSel + NUM_QUESTIONS - 1) % NUM_QUESTIONS; drawList(QSHORT, NUM_QUESTIONS, dSel, dListRow, LAST_ROW - 1); }
      if (pressed(1)) { dSel = (dSel + 1) % NUM_QUESTIONS;                 drawList(QSHORT, NUM_QUESTIONS, dSel, dListRow, LAST_ROW - 1); }
      if (pressed(2)) {
        dNewQ = dSel;
        dState = D_PICK_ANS;
        ui.fillScreen(COL_BG);
        header("TEACH ME");
        char buf[24];
        snprintf(buf, sizeof(buf), "For a %s:", ANIMALS[dNewAnimal]);
        txt(4 * S, rowY(1), buf, COL_WARN);
        wrapText(4 * S, rowY(3), QUESTIONS[dNewQ], COL_TEXT);
        txt(4 * S, rowY(LAST_ROW), "A:Yes  B:No", COL_MUTE);
      }
      break;

    case D_PICK_ANS:
      if (pressed(0) || pressed(1)) {
        dLearn(pressed(0));
        dState = D_DONE;
        char buf[24];
        snprintf(buf, sizeof(buf), "I know %d now", dKnown());
        dDrawEnd("Thanks!", buf);
        beep(1200, 80);
      }
      break;

    case D_WIN:
    case D_DONE:
      if (pressed(2)) { dNode = 0; dAsked = 0; dShowNode(); }
      break;
  }
  return true;
}

// ============================== SELF-DRIVING RACER ==============================
// Q-learning. The car sees its lane and the next two rows of road, and learns
// which move (left, stay, right) leads to surviving longer. Train it, watch the
// learning curve rise, then race it yourself.

#define RC_ROWS 8
#define RC_MAX  300            // a run ends at this score
uint8_t rcRoad[RC_ROWS];       // obstacle lane per row, 255 = empty
int rcLane = 1, rcScore = 0;
float rcQ[48][3];
uint32_t rcRuns = 0;
int rcBest = 0;
uint16_t rcHist[40];           // average score per 10 training runs
uint8_t rcHistN = 0;

enum RcState { RC_HUB, RC_WATCH, RC_DRIVE, RC_RESULT };
RcState rcState = RC_HUB;
int rcSel = 0;
unsigned long rcNextTick = 0;
int rcPending = 1;             // player's next move: 0 left, 1 stay, 2 right
bool rcPlayer = false;

const char* RC_ITEMS[] = {"Train 20 runs", "Watch AI drive", "You drive", "Forget training"};
const int RC_NUM_ITEMS = 4;

void rcLoad() {
  memset(rcQ, 0, sizeof(rcQ));
  rcRuns = 0; rcBest = 0; rcHistN = 0;
  prefs.begin("racer", true);
  if (prefs.getBytesLength("q") == sizeof(rcQ)) {
    prefs.getBytes("q", rcQ, sizeof(rcQ));
    rcRuns = prefs.getULong("runs", 0);
    rcBest = prefs.getInt("best", 0);
    rcHistN = prefs.getUChar("hn", 0);
    if (rcHistN > 40) rcHistN = 0;
    if (rcHistN) prefs.getBytes("h", rcHist, sizeof(uint16_t) * rcHistN);
  }
  prefs.end();
}

void rcSave() {
  prefs.begin("racer", false);
  prefs.putBytes("q", rcQ, sizeof(rcQ));
  prefs.putULong("runs", rcRuns);
  prefs.putInt("best", rcBest);
  prefs.putUChar("hn", rcHistN);
  if (rcHistN) prefs.putBytes("h", rcHist, sizeof(uint16_t) * rcHistN);
  prefs.end();
}

void rcReset() {
  for (int r = 0; r < RC_ROWS; r++) rcRoad[r] = 255;
  rcLane = 1;
  rcScore = 0;
}

int rcStateIdx() {
  int o1 = rcRoad[RC_ROWS - 2] == 255 ? 3 : rcRoad[RC_ROWS - 2];
  int o2 = rcRoad[RC_ROWS - 3] == 255 ? 3 : rcRoad[RC_ROWS - 3];
  return rcLane * 16 + o1 * 4 + o2;
}

// Move the car, scroll the road, return true on a crash.
bool rcStep(int action) {
  rcLane += action - 1;
  if (rcLane < 0) rcLane = 0;
  if (rcLane > 2) rcLane = 2;
  for (int r = RC_ROWS - 1; r > 0; r--) rcRoad[r] = rcRoad[r - 1];
  rcRoad[0] = (frand() < 0.55f) ? (esp_random() % 3) : 255;
  rcScore++;
  return rcRoad[RC_ROWS - 1] == rcLane;
}

int rcBestAction(int s) {
  int best = 1;
  for (int a = 0; a < 3; a++) if (rcQ[s][a] > rcQ[s][best]) best = a;
  return best;
}

// One training run with exploration. Returns the score reached.
int rcTrainRun(float eps) {
  rcReset();
  while (true) {
    int s = rcStateIdx();
    int a = (frand() < eps) ? (int)(esp_random() % 3) : rcBestAction(s);
    bool crashed = rcStep(a);
    float r = crashed ? -10.0f : 1.0f;
    float target = r;
    if (!crashed) {
      int s2 = rcStateIdx();
      float m = rcQ[s2][0];
      for (int b = 1; b < 3; b++) if (rcQ[s2][b] > m) m = rcQ[s2][b];
      target += 0.9f * m;
    }
    rcQ[s][a] += 0.3f * (target - rcQ[s][a]);
    if (crashed || rcScore >= RC_MAX) return rcScore;
  }
}

void rcDrawChart(int y0, int y1) {
  uiDirty = true;
  int x0 = 4 * S, x1 = W - 4 * S;
  ui.drawRect(x0, y0, x1 - x0, y1 - y0, COL_MUTE);
  if (rcHistN == 0) {
    ctxt((y0 + y1) / 2 - 4 * S, "Train to see it learn", COL_MUTE);
    return;
  }
  txt(x0 + 2 * S, y0 + 2 * S, "avg score", COL_MUTE);
  int px = -1, py = -1;
  for (int i = 0; i < rcHistN; i++) {
    int x = x0 + 2 + (rcHistN == 1 ? 0 : (i * (x1 - x0 - 4)) / (rcHistN - 1));
    int y = y1 - 2 - (rcHist[i] * (y1 - y0 - 4)) / RC_MAX;
    if (px >= 0) ui.drawLine(px, py, x, y, COL_GOOD);
    ui.fillCircle(x, y, S, COL_GOOD);
    px = x; py = y;
  }
}

void rcDrawHub() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("RACER");
  char buf[28];
  snprintf(buf, sizeof(buf), "Runs %lu Best %d", (unsigned long)rcRuns, rcBest);
  txt(4 * S, rowY(1), buf, COL_TEXT);
  rcDrawChart(rowY(2), rowY(LAST_ROW - 5) - 3 * S);
  drawList(RC_ITEMS, RC_NUM_ITEMS, rcSel, LAST_ROW - 4, LAST_ROW - 1);
  txt(4 * S, rowY(LAST_ROW), "A/B move TEST pick", COL_MUTE);
}

void rcDrawRoad() {
  uiDirty = true;
  const int top = 48, cellH = 54, laneW = 70, x0 = 15;
  ui.fillRoundRect(8, 42, 224, 226, 12, COL_SEL);
  ui.fillRect(x0, top, laneW * 3, cellH * 4, 0x39E7);
  for (int l = 1; l < 3; ++l)
    for (int y = top - (rcScore % 3) * 6; y < top + cellH * 4; y += 24)
      ui.fillRect(x0 + l * laneW - 1, max(top, y), 2,
                   min(12, top + cellH * 4 - max(top, y)), COL_TEXT);
  // Show the four nearest rows. Physics still uses the original eight-row road.
  for (int v = 0; v < 4; ++v) {
    int r = RC_ROWS - 4 + v;
    if (rcRoad[r] != 255)
      drawSprite(OBSTACLE, x0 + rcRoad[r] * laneW + 12,
                  top + v * cellH + 5, 46, 44);
  }
  drawSprite(CAR, x0 + rcLane * laneW + 12,
             top + cellH * 3 + 2, 46, 50);
}

void rcDrawRunFrame() {
  uiDirty = true;
  ui.fillRect(0, 274, W, 46, COL_BG);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s %d / %d", rcPlayer ? "YOU" : "AI", rcScore, RC_MAX);
  ctxt(278, buf, COL_TEXT);
  ctxtSz(306, rcPlayer ? "A: left  B: right  RESET: back" : "Watch it learn!  RESET: back", COL_MUTE, 1);
  rcDrawRoad();
}

void rcBeginRun(bool player) {
  rcPlayer = player;
  rcReset();
  rcPending = 1;
  rcState = player ? RC_DRIVE : RC_WATCH;
  ui.fillScreen(COL_BG);
  header(player ? "YOU DRIVE" : "AI DRIVES");
  rcDrawRunFrame();
  rcNextTick = millis() + 600;
}

void rcEndRun() {
  uiDirty = true;
  rcState = RC_RESULT;
  if (!rcPlayer && rcScore > rcBest) { rcBest = rcScore; rcSave(); }
  ui.fillRect(0, H / 2 - 12 * S, W, 34 * S, COL_ACCENT);
  char buf[28];
  snprintf(buf, sizeof(buf), rcScore >= RC_MAX ? "Finished! %d" : "Crashed at %d", rcScore);
  ctxt(H / 2 - 8 * S, buf, COL_TEXT);
  if (rcPlayer) snprintf(buf, sizeof(buf), "AI best: %d", rcBest);
  else snprintf(buf, sizeof(buf), "TEST: back");
  ctxt(H / 2 + 6 * S, buf, COL_TEXT);
  beep(rcScore >= RC_MAX ? 1200 : 300, 150);
}

void rcStart() {
  rcLoad();
  rcState = RC_HUB;
  rcSel = 0;
  rcDrawHub();
}

bool rcUpdate() {
  switch (rcState) {
    case RC_HUB:
      if (pressed(3)) { rcSave(); return false; }
      if (pressed(0)) { rcSel = (rcSel + RC_NUM_ITEMS - 1) % RC_NUM_ITEMS; drawList(RC_ITEMS, RC_NUM_ITEMS, rcSel, LAST_ROW - 4, LAST_ROW - 1); }
      if (pressed(1)) { rcSel = (rcSel + 1) % RC_NUM_ITEMS;                drawList(RC_ITEMS, RC_NUM_ITEMS, rcSel, LAST_ROW - 4, LAST_ROW - 1); }
      if (pressed(2)) {
        if (rcSel == 0) {                       // train
          ui.fillRect(0, H / 2 - 8 * S, W, 16 * S, COL_ACCENT);
          ctxt(H / 2 - 4 * S, "Training...", COL_TEXT);
          presentScreen();
          for (int batch = 0; batch < 2; batch++) {
            long sum = 0;
            for (int i = 0; i < 10; i++) {
              float eps = 0.5f * powf(0.97f, (float)rcRuns);
              if (eps < 0.02f) eps = 0.02f;
              sum += rcTrainRun(eps);
              rcRuns++;
              yield();
            }
            if (rcHistN == 40) { memmove(rcHist, rcHist + 1, sizeof(uint16_t) * 39); rcHistN = 39; }
            rcHist[rcHistN++] = sum / 10;
          }
          Serial.printf("Racer trained: %lu runs, last avg %d\n", (unsigned long)rcRuns, rcHist[rcHistN - 1]);
          rcSave();
          rcDrawHub();
        } else if (rcSel == 1) {
          rcBeginRun(false);
        } else if (rcSel == 2) {
          rcBeginRun(true);
        } else {
          memset(rcQ, 0, sizeof(rcQ));
          rcRuns = 0; rcBest = 0; rcHistN = 0;
          rcSave();
          flashMessage("Training cleared");
          rcDrawHub();
        }
      }
      break;

    case RC_WATCH:
    case RC_DRIVE:
      if (pressed(3)) { rcState = RC_HUB; rcDrawHub(); break; }
      if (rcPlayer) {
        if (pressed(0)) rcPending = 0;
        if (pressed(1)) rcPending = 2;
      }
      if (millis() >= rcNextTick) {
        int a = rcPlayer ? rcPending : rcBestAction(rcStateIdx());
        rcPending = 1;
        bool crashed = rcStep(a);
        rcDrawRunFrame();
        if (crashed || rcScore >= RC_MAX) rcEndRun();
        rcNextTick = millis() + (rcPlayer ? max(120, 260 - rcScore) : 110);
      }
      break;

    case RC_RESULT:
      if (pressed(2) || pressed(3)) { rcState = RC_HUB; rcDrawHub(); }
      break;
  }
  return true;
}

#include "camera_activity.h"

// ============================== LAUNCHER ==============================

struct Game {
  const char* name;
  bool ready;
  void (*start)();
  bool (*update)();   // return false to go back to the menu
};

Game games[] = {
  {"Mind Reader",  true,  mrStart, mrUpdate},
  {"Pet Trainer",  true,  petStart, petUpdate},
  {"Detective",    true,  dStart, dUpdate},
  {"Racer",        true,  rcStart, rcUpdate},
  {"Camera Lab",   true,  camStart, camUpdate},
};
const int NUM_GAMES = sizeof(games) / sizeof(games[0]);

enum Mode { MODE_MENU, MODE_GAME };
Mode mode = MODE_MENU;
int sel = 0;
int current = -1;

void drawMenu() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  ui.fillRoundRect(8, 5, 224, 88, 12, COL_TEXT);
  ui.drawRGBBitmap((W - MILTALK_LOGO_WIDTH) / 2, 9,
                   MILTALK_LOGO_PIXELS, MILTALK_LOGO_WIDTH, MILTALK_LOGO_HEIGHT);
  const uint16_t colors[5] = {COL_ACCENT, COL_WARN, 0xAD9F, COL_GOOD, COL_BAD};
  for (int i = 0; i < NUM_GAMES; ++i) {
    int y = 101 + i * 33;
    ui.fillRoundRect(10, y, 220, 29, 9, i == sel ? colors[i] : COL_SEL);
    ui.fillCircle(25, y + 14, 8, i == sel ? COL_BG : colors[i]);
    char n[3]; snprintf(n, sizeof(n), "%d", i + 1);
    txtSz(22, y + 11, n, i == sel ? colors[i] : COL_BG, 1);
    txtSz(41, y + 7, games[i].name, i == sel ? COL_BG : COL_TEXT, 2);
  }
  ctxtSz(280, "A/B: move   TEST: play", COL_TEXT, 1);
  char buf[40];
  snprintf(buf, sizeof(buf), "RESET: buddy (%s)", BUDDY_NAMES[selectedBuddy]);
  ctxtSz(302, buf, COL_MUTE, 1);
}

void drawSplash() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  ctxtSz(H / 2 - 16 * S, "MILTALK", COL_ACCENT, 2 * S);
  ctxt(H / 2 + 4 * S, "AI games", COL_MUTE);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nMiltalk Console v0.2 - Rev C");

  for (int i = 0; i < 4; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  displayBegin();
  Serial.printf("Display ready: %d x %d\n", W, H);

  buddyLoad();
  drawBuddyPicker();
  presentScreen();
}

void loop() {
  readButtons();
  if (choosingBuddy) {
    if (pressed(0)) { selectedBuddy = (selectedBuddy + 2) % 3; drawBuddyPicker(); }
    else if (pressed(1)) { selectedBuddy = (selectedBuddy + 1) % 3; drawBuddyPicker(); }
    else if (pressed(2)) { buddySave(); choosingBuddy = false; drawMenu(); }
    presentScreen();
    delay(5);
    return;
  }

  if (mode == MODE_MENU) {
    if (pressed(3)) {
      choosingBuddy = true;
      drawBuddyPicker();
      presentScreen();
      return;
    }
    if (pressed(0)) { sel = (sel + NUM_GAMES - 1) % NUM_GAMES; drawMenu(); beep(700, 20); }
    if (pressed(1)) { sel = (sel + 1) % NUM_GAMES;             drawMenu(); beep(700, 20); }
    if (pressed(2)) {
      if (games[sel].ready) {
        current = sel;
        mode = MODE_GAME;
        Serial.printf("Starting %s\n", games[current].name);
        games[current].start();
      } else {
        ui.fillRect(0, H / 2 - 8 * S, W, 16 * S, COL_ACCENT);
        ctxt(H / 2 - 4 * S, "Coming soon", COL_TEXT);
        screenPause(700);
        drawMenu();
      }
    }
  } else {
    if (!games[current].update()) {
      mode = MODE_MENU;
      drawMenu();
    }
  }

  presentScreen();
  delay(5);
}



#pragma once
// The original camera map, 16x16 mean-centred features, two-class nearest-centroid
// model and ESP-NOW model-transfer wire format are retained.
#define SCR_W 240
#define SCR_H 320
#define COL_CARD COL_SEL
constexpr uint16_t CAPTURE_HOLD_MS = 3000;

void centeredText(const char* text, int y, uint8_t size, uint16_t color) {
  ctxtSz(y, text, color, size);
}
void pageFooter(const char* text) {
  ui.fillRoundRect(8, 290, 224, 23, 8, COL_SEL);
  ctxtSz(298, text, COL_MUTE, 1);
}
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y2_GPIO_NUM     11
#define Y3_GPIO_NUM      9
#define Y4_GPIO_NUM      8
#define Y5_GPIO_NUM     10
#define Y6_GPIO_NUM     12
#define Y7_GPIO_NUM     18
#define Y8_GPIO_NUM     17
#define Y9_GPIO_NUM     16
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

#define FEAT_W 16
#define FEAT_H 16
#define FEAT_LEN (FEAT_W * FEAT_H)
#define MAX_EXAMPLES 20

float sumA[FEAT_LEN];
float sumB[FEAT_LEN];
int   countA = 0;
int   countB = 0;

String lastPrediction = "-";
int    lastConfidence  = 0;

// ----------------- FEEDBACK STATE -----------------
bool  feedbackMode = false;      // true right after a TEST, waiting for approve/disprove
char  predClass = 0;             // 'A' or 'B' - the class the kit last predicted
float lastFeat[FEAT_LEN];        // feature vector from the last test, reused for learning

#define KIT_ID        1          // trainer and tester must match
#define PROTO_VERSION 1
#define PKT_MAGIC     0xBEEF
#define PKT_CHUNK     1
#define PKT_ACK       2
#define CHUNK_DATA    200

typedef struct __attribute__((packed)) {
  uint16_t magic; uint8_t type; uint8_t kitId; uint8_t transferId;
  uint8_t seq; uint8_t total; uint16_t len;
  uint32_t crc;                  // CRC32 of the whole payload
  uint8_t data[CHUNK_DATA];
} ChunkPacket;

typedef struct __attribute__((packed)) {
  uint16_t magic; uint8_t type; uint8_t kitId; uint8_t transferId; uint8_t seq;
} AckPacket;

typedef struct __attribute__((packed)) {
  uint16_t version; uint16_t featLen;
  int32_t  countA;  int32_t  countB;
  float sumA[FEAT_LEN];
  float sumB[FEAT_LEN];
} WeightsPayload;

static_assert(sizeof(ChunkPacket) == 213, "Keep the TESTER packet format unchanged");
static_assert(sizeof(AckPacket) == 6, "Keep the TESTER acknowledgement format unchanged");
static_assert(sizeof(WeightsPayload) == 2060, "Keep the TESTER model format unchanged");

uint32_t crc32(const uint8_t* d, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & -(c & 1));
  }
  return ~c;
}

uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool espNowReady = false;
WeightsPayload txPayload;
uint8_t txTransferId = 0;
volatile int     lastAckSeq = -1;
volatile uint8_t lastAckId  = 0;

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED 0x%x\n", err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_whitebal(s, 0);
    s->set_awb_gain(s, 0);
    s->set_exposure_ctrl(s, 0);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 0);
    s->set_agc_gain(s, 5);
    s->set_aec_value(s, 300);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
  }
  return true;
}

void drawFramePreview(camera_fb_t* fb) {
  if (!fb || fb->format != PIXFORMAT_GRAYSCALE || !ui.getBuffer()) return;
  uint32_t started = millis();
  int srcW = fb->width, srcH = fb->height;
  if (srcW <= 0 || srcH <= 0 || fb->len < (size_t)srcW * srcH) return;
  // Preserve the camera's aspect ratio: 160x120 becomes 240x180.
  int dstW = SCR_W;
  int dstH = srcH * dstW / srcW;
  if (dstH > 224) { dstH = 224; dstW = srcW * dstH / srcH; }
  int offsetX = (SCR_W - dstW) / 2;
  int offsetY = 48 + (224 - dstH) / 2;
  ui.fillScreen(COL_BG);
  centeredText("I SAW THIS!", 17, 2, COL_ACCENT);
  uint16_t* frame = ui.getBuffer();
  for (int dy = 0; dy < dstH; dy++) {
    int sy = dy * srcH / dstH;
    const uint8_t* source = fb->buf + sy * srcW;
    uint16_t* row = frame + (offsetY + dy) * SCR_W + offsetX;
    for (int dx = 0; dx < dstW; dx++) {
      uint8_t g = source[dx * srcW / dstW];
      row[dx] = ((g & 0xF8) << 8) | ((g & 0xFC) << 3) | (g >> 3);
    }
  }
  pageFooter("A little snapshot of your world");
  presentScreen();
  Serial.printf("Preview build + transfer: %lu ms\n", (unsigned long)(millis() - started));
}

// ----------------- FEATURE EXTRACTION -----------------
bool captureFeatures(float* out) {
  for (int i = 0; i < 3; i++) {
    camera_fb_t* tmp = esp_camera_fb_get();
    if (tmp) esp_camera_fb_return(tmp);
    delay(30);
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Capture FAILED"); return false; }

  int srcW = fb->width, srcH = fb->height;
  if (fb->format != PIXFORMAT_GRAYSCALE || srcW < FEAT_W || srcH < FEAT_H ||
      fb->len < (size_t)srcW * srcH) {
    esp_camera_fb_return(fb);
    Serial.println("Invalid grayscale frame");
    return false;
  }
  uint8_t* px = fb->buf;
  int blockW = srcW / FEAT_W; if (blockW < 1) blockW = 1;
  int blockH = srcH / FEAT_H; if (blockH < 1) blockH = 1;

  for (int fy = 0; fy < FEAT_H; fy++) {
    for (int fx = 0; fx < FEAT_W; fx++) {
      long sum = 0; int n = 0;
      for (int by = 0; by < blockH; by++) {
        for (int bx = 0; bx < blockW; bx++) {
          int sx = fx * blockW + bx;
          int sy = fy * blockH + by;
          if (sx < srcW && sy < srcH) { sum += px[sy * srcW + sx]; n++; }
        }
      }
      float avg = (n > 0) ? (float)sum / n : 0.0f;
      out[fy * FEAT_W + fx] = avg / 255.0f;
    }
  }
  drawFramePreview(fb);   // show what the camera saw
  screenPause(CAPTURE_HOLD_MS);
  esp_camera_fb_return(fb);

  float mean = 0;
  for (int i = 0; i < FEAT_LEN; i++) mean += out[i];
  mean /= FEAT_LEN;
  for (int i = 0; i < FEAT_LEN; i++) out[i] -= mean;
  return true;
}

// ----------------- CLASSIFIER -----------------
void resetTraining() {
  for (int i = 0; i < FEAT_LEN; i++) { sumA[i] = 0; sumB[i] = 0; }
  countA = 0; countB = 0;
  lastPrediction = "-"; lastConfidence = 0;
  feedbackMode = false; predClass = 0;
  Serial.println("=== RESET ===");
}

void trainExample(bool isClassA) {
  if (isClassA && countA >= MAX_EXAMPLES) { Serial.println("A full"); return; }
  if (!isClassA && countB >= MAX_EXAMPLES) { Serial.println("B full"); return; }
  float feat[FEAT_LEN];
  if (!captureFeatures(feat)) return;
  if (isClassA) {
    for (int i = 0; i < FEAT_LEN; i++) sumA[i] += feat[i];
    countA++;
    Serial.printf("Trained A: %d\n", countA);
  } else {
    for (int i = 0; i < FEAT_LEN; i++) sumB[i] += feat[i];
    countB++;
    Serial.printf("Trained B: %d\n", countB);
  }
}

float distToCentroid(const float* feat, const float* sum, int count) {
  if (count == 0) return 1e9;
  float d = 0;
  for (int i = 0; i < FEAT_LEN; i++) {
    float c = sum[i] / count;
    float diff = feat[i] - c;
    d += diff * diff;
  }
  return sqrt(d);
}

void testExample() {
  if (countA == 0 || countB == 0) {
    lastPrediction = "train both"; lastConfidence = 0;
    Serial.println("Train both A and B first.");
    return;
  }
  if (!captureFeatures(lastFeat)) return;   // store into lastFeat for feedback reuse
  float dA = distToCentroid(lastFeat, sumA, countA);
  float dB = distToCentroid(lastFeat, sumB, countB);
  bool aWins = dA < dB;
  float nr = aWins ? dA : dB;
  float fr = aWins ? dB : dA;
  float conf = (fr + nr > 0) ? (fr - nr) / (fr + nr) * 100.0f : 0;
  lastPrediction = aWins ? "SET A" : "SET B";
  predClass = aWins ? 'A' : 'B';
  lastConfidence = (int)conf;
  Serial.printf("Predict %s (%.0f%%)\n", lastPrediction.c_str(), conf);
  feedbackMode = true;   // enter feedback mode after a test
}

// ----------------- FEEDBACK LEARNING -----------------
void learnFromFeedback(char cls) {
  if (cls == 'A') {
    if (countA < MAX_EXAMPLES) { for (int i = 0; i < FEAT_LEN; i++) sumA[i] += lastFeat[i]; countA++; }
  } else {
    if (countB < MAX_EXAMPLES) { for (int i = 0; i < FEAT_LEN; i++) sumB[i] += lastFeat[i]; countB++; }
  }
}

void handleApprove() {
  learnFromFeedback(predClass);   // right: reinforce predicted class
  Serial.printf("Approved. Reinforced %c (A:%d B:%d)\n", predClass, countA, countB);
  feedbackMode = false;
}

void handleDisprove() {
  char correct = (predClass == 'A') ? 'B' : 'A';   // wrong: it was the other class
  learnFromFeedback(correct);
  Serial.printf("Corrected to %c (A:%d B:%d)\n", correct, countA, countB);
  feedbackMode = false;
}

void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != sizeof(AckPacket)) return;
  const AckPacket* a = (const AckPacket*)data;
  if (a->magic != PKT_MAGIC || a->type != PKT_ACK || a->kitId != KIT_ID) return;
  lastAckId  = a->transferId;
  lastAckSeq = a->seq;
}

bool initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init FAILED"); return false; }
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("ESP-NOW add peer FAILED"); return false; }
  Serial.println("ESP-NOW ready");
  return true;
}

#define SEND_OK          0
#define SEND_NOT_TRAINED 1
#define SEND_NO_RADIO    2
#define SEND_NO_ACK      3

int sendWeights() {
  if (countA == 0 || countB == 0) return SEND_NOT_TRAINED;
  if (!espNowReady) return SEND_NO_RADIO;

  // Read-only copy of the existing training state
  txPayload.version = PROTO_VERSION;
  txPayload.featLen = FEAT_LEN;
  txPayload.countA  = countA;
  txPayload.countB  = countB;
  memcpy(txPayload.sumA, sumA, sizeof(sumA));
  memcpy(txPayload.sumB, sumB, sizeof(sumB));

  const uint8_t* buf = (const uint8_t*)&txPayload;
  size_t total = sizeof(txPayload);
  uint8_t nChunks = (total + CHUNK_DATA - 1) / CHUNK_DATA;
  uint32_t crc = crc32(buf, total);
  txTransferId++;
  if (txTransferId == 0) txTransferId = 1;
  unsigned long t0 = millis();

  for (uint8_t seq = 0; seq < nChunks; seq++) {
    ChunkPacket p = {};
    p.magic = PKT_MAGIC; p.type = PKT_CHUNK; p.kitId = KIT_ID;
    p.transferId = txTransferId; p.seq = seq; p.total = nChunks; p.crc = crc;
    size_t off = (size_t)seq * CHUNK_DATA;
    p.len = min((size_t)CHUNK_DATA, total - off);
    memcpy(p.data, buf + off, p.len);

    bool acked = false;
    for (int attempt = 0; attempt < 5 && !acked; attempt++) {
      lastAckSeq = -1;
      esp_now_send(broadcastAddr, (uint8_t*)&p, sizeof(p));
      unsigned long ts = millis();
      while (millis() - ts < 50) {
        if (lastAckId == txTransferId && lastAckSeq == seq) { acked = true; break; }
        delay(1);
      }
    }
    if (!acked) { Serial.printf("Send FAILED at chunk %d\n", seq); return SEND_NO_ACK; }
  }
  Serial.printf("Weights sent (A:%d B:%d) in %lu ms\n", countA, countB, millis() - t0);
  return SEND_OK;
}


bool camReady = false;
bool camWaitRelease = true;
uint8_t camGesture = 0;
unsigned long camGestureStart = 0, camComboStart = 0;
String camNote = "Teach it two different things!";

void camDrawPage() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("CAMERA LAB");
  ctxtSz(35, "See it. Teach it. Test it!", COL_ACCENT, 1);
  ui.fillRoundRect(10, 53, 105, 60, 12, COL_WARN);
  ui.fillRoundRect(125, 53, 105, 60, 12, 0xAD9F);
  txtSz(21, 63, "SET A", COL_BG, 1);
  txtSz(137, 63, "SET B", COL_BG, 1);
  char buf[40];
  snprintf(buf, sizeof(buf), "%d / 20", countA);
  txtSz(21, 84, buf, COL_BG, 2);
  snprintf(buf, sizeof(buf), "%d / 20", countB);
  txtSz(137, 84, buf, COL_BG, 2);
  ui.fillRoundRect(10, 124, 220, 94, 14, COL_SEL);
  ctxtSz(135, "MY GUESS", COL_MUTE, 1);
  ctxtSz(155, lastPrediction.c_str(), COL_TEXT, 3);
  if (predClass) snprintf(buf, sizeof(buf), "Separation score: %d%%", lastConfidence);
  else snprintf(buf, sizeof(buf), "Train A and B, then press TEST");
  ctxtSz(199, buf, COL_ACCENT, 1);
  if (!camReady) {
    ctxtSz(232, "Camera unavailable. Check ribbon/PSRAM.", COL_BAD, 1);
    ctxtSz(252, "TEST: retry   RESET: menu", COL_TEXT, 1);
  } else if (feedbackMode) {
    ctxt(231, "Was I right?", COL_WARN);
    ctxtSz(257, "A: YES  B: NO  TEST: skip", COL_TEXT, 1);
  } else {
    ctxtSz(232, camNote.c_str(), COL_TEXT, 1);
    ctxtSz(255, "A: train A  B: train B  TEST: guess", COL_ACCENT, 1);
  }
  ctxtSz(279, "RESET: menu / hold 1.5s: clear", COL_MUTE, 1);
  ctxtSz(302, "Hold ALL 4: send model to TESTER", COL_MUTE, 1);
}

void camStart() {
  camWaitRelease = true;
  camGesture = 0;
  camComboStart = 0;
  camNote = "Teach it two different things!";
  ui.fillScreen(COL_BG);
  header("CAMERA LAB");
  ctxt(138, "Starting camera...", COL_ACCENT);
  presentScreen();
  if (!camReady) {
    camReady = initCamera();
    if (camReady) {
      for (int i = 0; i < 10; ++i) {
        camera_fb_t* frame = esp_camera_fb_get();
        if (frame) esp_camera_fb_return(frame);
        delay(30);
      }
    }
  }
  camDrawPage();
}

void camStop() {
  if (camReady) esp_camera_deinit();
  camReady = false;
  if (espNowReady) {
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    espNowReady = false;
  }
  feedbackMode = false;
}

bool camUpdate() {
  uint8_t mask = 0;
  for (int i = 0; i < 4; ++i)
    if (held(i)) mask |= (1 << i);
  if (camWaitRelease) {
    if (!mask) {
      camWaitRelease = false;
      camGesture = 0;
      camComboStart = 0;
    }
    return true;
  }
  if (!camReady) {
    if (pressed(3)) { camStop(); return false; }
    if (pressed(2)) camStart();
    return true;
  }

  // Camera single-button actions happen on release. Combining four buttons
  // therefore never accidentally trains, clears or captures before sending.
  if (mask) {
    if (!camGesture) camGestureStart = millis();
    camGesture |= mask;
    if (mask == 15) {
      if (!camComboStart) camComboStart = millis();
      if (millis() - camComboStart >= 800) {
        ui.fillScreen(COL_BG);
        header("MODEL TRANSFER");
        ctxt(133, "SENDING...", COL_ACCENT);
        presentScreen();
        if (!espNowReady) espNowReady = initEspNow();
        int result = sendWeights();
        const char* note = result == SEND_OK ? "Model sent!" :
          result == SEND_NOT_TRAINED ? "Train both sets first" :
          result == SEND_NO_RADIO ? "Radio not ready" : "No TESTER acknowledgement";
        flashMessage(note);
        camNote = note;
        camWaitRelease = true;
        camDrawPage();
      }
    } else camComboStart = 0;
    if (mask == 8 && camGesture == 8 && millis() - camGestureStart >= 1500) {
      resetTraining();
      camNote = "Training cleared. Let's start again!";
      camWaitRelease = true;
      camDrawPage();
    }
    return true;
  }

  uint8_t action = camGesture;
  camGesture = 0;
  camComboStart = 0;
  if (!action) return true;
  if (action == 8) { camStop(); return false; }
  if (action != 1 && action != 2 && action != 4) return true;

  if (feedbackMode) {
    if (action == 1) { handleApprove(); camNote = "Great! I learned from your feedback."; }
    else if (action == 2) { handleDisprove(); camNote = "Thanks! You corrected my guess."; }
    else { feedbackMode = false; camNote = "Feedback skipped."; }
  } else if (action == 1 || action == 2) {
    bool isA = action == 1;
    int before = isA ? countA : countB;
    trainExample(isA);
    int after = isA ? countA : countB;
    camNote = before >= MAX_EXAMPLES ? "Set full! Hold RESET to start over." :
      after > before ? "Example added. Try a new angle!" : "Capture failed. Try again.";
  } else {
    testExample();
    if (!countA || !countB) camNote = "Add examples to BOTH sets first.";
  }
  camWaitRelease = true;
  camDrawPage();
  return true;
}


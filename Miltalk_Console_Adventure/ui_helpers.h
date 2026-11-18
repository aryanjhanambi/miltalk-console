#pragma once

const char* BUDDY_NAMES[3] = {"Dragon", "Princess", "Batman"};
const Sprite* BUDDY_SPRITES[3] = {&DRAGON, &PRINCESS, &HERO};
int selectedBuddy = 0;
bool choosingBuddy = true;

void drawSprite(const Sprite& sprite, int x, int y, int boxW, int boxH) {
  uiDirty = true;
  int dw = boxW;
  int dh = sprite.height * dw / sprite.width;
  if (dh > boxH) { dh = boxH; dw = sprite.width * dh / sprite.height; }
  int ox = x + (boxW - dw) / 2;
  int oy = y + (boxH - dh) / 2;
  uint16_t* pixels = ui.getBuffer();
  for (int dy = 0; dy < dh; ++dy) {
    int py = oy + dy;
    if (py < 0 || py >= H) continue;
    for (int dx = 0; dx < dw; ++dx) {
      int px = ox + dx;
      if (px < 0 || px >= W) continue;
      int sy = dy * sprite.height / dh;
      int sx = dx * sprite.width / dw;
      pixels[py * W + px] = pgm_read_word(sprite.pixels + sy * sprite.width + sx);
    }
  }
}

void drawSpark(int x, int y, uint16_t color) {
  ui.drawFastHLine(x - 3, y, 7, color);
  ui.drawFastVLine(x, y - 3, 7, color);
}

void buddyLoad() {
  Preferences profile;
  profile.begin("mtprofile", true);
  selectedBuddy = profile.getUChar("buddy", 0);
  profile.end();
  if (selectedBuddy > 2) selectedBuddy = 0;
}

void buddySave() {
  Preferences profile;
  profile.begin("mtprofile", false);
  profile.putUChar("buddy", selectedBuddy);
  profile.end();
}

void drawBuddyPicker() {
  uiDirty = true;
  ui.fillScreen(COL_BG);
  header("CHOOSE YOUR BUDDY");
  ctxt(37, "Your AI adventure starts here", COL_MUTE);
  ui.fillRoundRect(40, 62, 160, 151, 16, COL_TEXT);
  drawSprite(*BUDDY_SPRITES[selectedBuddy], 48, 69, 144, 137);
  txtSz(15, 130, "<", COL_ACCENT, 3);
  txtSz(208, 130, ">", COL_ACCENT, 3);
  ctxtSz(225, BUDDY_NAMES[selectedBuddy], COL_TEXT, 3);
  for (int i = 0; i < 3; ++i)
    ui.fillCircle(101 + i * 19, 260, i == selectedBuddy ? 5 : 3,
                  i == selectedBuddy ? COL_ACCENT : COL_MUTE);
  ctxt(279, "A / B: choose", COL_MUTE);
  ui.fillRoundRect(14, 300, 212, 18, 7, COL_ACCENT);
  ctxtSz(304, "TEST: let's play!", COL_BG, 1);
  drawSpark(20, 72, COL_WARN);
  drawSpark(220, 218, COL_GOOD);
}

void drawBuddyReaction(int mood) {
  uiDirty = true;
  ui.fillRoundRect(70, 52, 100, 101, 10, COL_TEXT);
  drawSprite(*BUDDY_SPRITES[selectedBuddy], 76, 55, 88, 96);
  uint16_t color = mood > 0 ? COL_GOOD : mood < 0 ? COL_BAD : COL_ACCENT;
  ui.fillCircle(195, 91, 17, color);
  ui.fillCircle(189, 86, 2, COL_BG);
  ui.fillCircle(201, 86, 2, COL_BG);
  if (mood > 0) {
    ui.drawLine(188, 95, 195, 100, COL_BG);
    ui.drawLine(195, 100, 202, 95, COL_BG);
  } else if (mood < 0) {
    ui.drawLine(188, 100, 195, 95, COL_BG);
    ui.drawLine(195, 95, 202, 100, COL_BG);
  } else ui.drawFastHLine(190, 97, 11, COL_BG);
}

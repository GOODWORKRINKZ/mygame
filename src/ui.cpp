#include "ui.h"

namespace Ui {

// Вертикальная граница между половинами экрана.
static const int SPLIT_X = 63;
static const int L0 = 0,  L1 = SPLIT_X - 1;
static const int R0 = SPLIT_X + 2, R1 = OLED_WIDTH - 1;

void header(Display& d, const char* left, const char* right) {
  d.text(0, 1, left, 1);
  if (right && right[0]) {
    int w = Display::textWidth(right, 1);
    d.text(OLED_WIDTH - w, 1, right, 1);
  }
  d.gfx().drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);
}

void footer(Display& d, const char* text) {
  d.gfx().drawFastHLine(0, 53, OLED_WIDTH, SSD1306_WHITE);
  if (text && text[0]) d.textCentered(56, text, 1);
}

void divider(Display& d) {
  d.gfx().drawFastVLine(SPLIT_X, 12, 40, SSD1306_WHITE);
}

void bar(Display& d, int x, int y, int w, int h, int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  d.gfx().drawRect(x, y, w, h, SSD1306_WHITE);
  int fillW = ((w - 2) * pct) / 100;
  if (fillW > 0) d.gfx().fillRect(x + 1, y + 1, fillW, h - 2, SSD1306_WHITE);
}

void dotsRow(Display& d, int cx, int y, int total, int filled) {
  if (total <= 0) return;
  const int step = 8;
  int x = cx - (total * step) / 2 + step / 2;
  for (int i = 0; i < total; i++) {
    if (i < filled) d.gfx().fillCircle(x + i * step, y, 3, SSD1306_WHITE);
    else            d.gfx().drawCircle(x + i * step, y, 3, SSD1306_WHITE);
  }
}

static void drawPanel(Display& d, int x0, int x1, const Panel& p) {
  int cx = (x0 + x1) / 2;

  if (p.active) {
    // подсвеченное имя — белая плашка с инверсным текстом
    d.gfx().fillRect(x0, 11, x1 - x0 + 1, 10, SSD1306_WHITE);
    d.gfx().setTextColor(SSD1306_BLACK);
    d.textCenteredIn(x0, x1, 12, p.name, 1);
    d.gfx().setTextColor(SSD1306_WHITE);
  } else {
    d.textCenteredIn(x0, x1, 12, p.name, 1);
  }

  d.textCenteredIn(x0, x1, 22, p.big, 2);

  if (p.sub && p.sub[0]) d.textCenteredIn(x0, x1, 40, p.sub, 1);

  if (p.dots >= 0 && p.dotsMax > 0) {
    dotsRow(d, cx, 49, p.dotsMax, p.dots);
  } else if (p.bar >= 0) {
    bar(d, x0 + 6, 46, (x1 - x0) - 11, 6, p.bar);
  }
}

void split(Display& d, const char* title, const char* right,
           const Panel& left, const Panel& rightPanel, const char* foot) {
  d.clear();
  header(d, title, right);
  divider(d);
  drawPanel(d, L0, L1, left);
  drawPanel(d, R0, R1, rightPanel);
  footer(d, foot);
  d.flush();
}

void solo(Display& d, const char* title, const char* right,
          const char* big, const char* line1, const char* line2, const char* foot) {
  d.clear();
  header(d, title, right);
  uint8_t size = (Display::textWidth(big, 3) <= OLED_WIDTH - 4) ? 3 : 2;
  d.textCentered(size == 3 ? 14 : 18, big, size);
  if (line1 && line1[0]) d.textCentered(39, line1, 1);
  if (line2 && line2[0]) d.textCentered(45, line2, 1);
  footer(d, foot);
  d.flush();
}

void banner(Display& d, const char* big, const char* sub, const char* foot) {
  d.clear();
  // рамка
  d.gfx().drawRect(0, 0, OLED_WIDTH, 52, SSD1306_WHITE);
  d.gfx().drawRect(2, 2, OLED_WIDTH - 4, 48, SSD1306_WHITE);
  uint8_t size = (Display::textWidth(big, 3) <= OLED_WIDTH - 10) ? 3 : 2;
  d.textCentered(size == 3 ? 12 : 16, big, size);
  if (sub && sub[0]) d.textCentered(size == 3 ? 38 : 34, sub, 1);
  if (foot && foot[0]) d.textCentered(56, foot, 1);
  d.flush();
}

void menuFrame(Display& d, const char* title, const char* const* items,
               const uint8_t* playerCounts, int count, int sel, const char* foot) {
  d.clear();
  char right[10];
  snprintf(right, sizeof right, "%d/%d", sel + 1, count);
  header(d, title, right);

  // окно на 4 строки, выбранный пункт держим внутри
  const int rows = 4;
  int top = sel - rows / 2;
  if (top > count - rows) top = count - rows;
  if (top < 0) top = 0;

  for (int r = 0; r < rows && (top + r) < count; r++) {
    int idx = top + r;
    int y = 14 + r * 10;
    bool cur = (idx == sel);
    if (cur) d.gfx().fillRect(0, y - 1, OLED_WIDTH, 10, SSD1306_WHITE);
    d.gfx().setTextColor(cur ? SSD1306_BLACK : SSD1306_WHITE);

    char tag[6];
    snprintf(tag, sizeof tag, "%dИ", playerCounts ? playerCounts[idx] : 2);
    d.text(2, y, cur ? ">" : " ", 1);
    d.text(10, y, items[idx], 1);
    d.text(OLED_WIDTH - 14, y, tag, 1);
  }
  d.gfx().setTextColor(SSD1306_WHITE);

  footer(d, foot);
  d.flush();
}

void bootFrame(Display& d, uint8_t phase, uint8_t progress) {
  d.clear();
  if (phase == 0) {
    d.textCentered(6, "LED", 3);
    d.textCentered(30, "АРКАДА", 2);
    bar(d, 14, 50, OLED_WIDTH - 28, 8, progress);
  } else if (phase == 1) {
    d.textCentered(10, "ГОТОВ", 3);
    d.textCentered(38, "32 ПИКСЕЛЯ ВЕСЕЛЬЯ", 1);
    for (uint8_t i = 0; i < 8; i++)
      d.gfx().fillCircle(8 + i * 16, 56, 3, SSD1306_WHITE);
  } else {
    d.textCentered(4, "УПРАВЛЕНИЕ", 1);
    d.text(2, 16, "ЗЕЛЕНАЯ = ИГРОК 1", 1);
    d.text(2, 26, "СИНЯЯ = ИГРОК 2", 1);
    d.text(2, 36, "МЕНЮ = ВЫБОР", 1);
    d.text(2, 46, "УДЕРЖ = НАЗАД/РЕСТАРТ", 1);
  }
  d.flushNow();
}

}  // namespace Ui

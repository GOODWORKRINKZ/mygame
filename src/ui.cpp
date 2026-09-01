#include "ui.h"

namespace Ui {

// ============================================================
//  Раскладка под двухцветный 0.96" OLED (жёлто-синий):
//
//    y= 0..15   ЖЁЛТАЯ зона   — шапка, имя игрока
//    y=16..17   мёртвая полоса (тут ничего не рисуем)
//    y=18..63   СИНЯЯ зона    — крупное число, sub, bar, подвал
//
//  Все размеры ниже подобраны так, чтобы ни один пиксель текста
//  или линии не попадал на стык жёлтой и синей зон.
// ============================================================

static const int SPLIT_X       = 63;
static const int L0 = 0,        L1 = SPLIT_X - 1;
static const int R0 = SPLIT_X + 2, R1 = OLED_WIDTH - 1;

// Y-координаты сетки.
// size=1 глиф = 7 строк + 1 строка под descenders (pg, gy, ц, щ, ю ...).
// size=2 = 14 строк + 1 строка под descenders.
// Нижний край экрана = 63. Для size=1 последняя строка глифа = y+6,
// descender = y+7 — значит курсор не выше y=56.
static const int Y_RULE_TOP    = 15;   // линия под шапкой (внутри жёлтой зоны)
static const int Y_NAME        = 16;   // имя игрока (size=1, y=18..24)
static const int Y_BIG         = 25;   // крупное число (size=2, y=27..40, desc y=41)
static const int Y_SUB         = 41;   // подпись (size=1, y=43..49, desc y=50)
static const int Y_BAR         = 49;   // полоска/bar (6px, y=51..56)
static const int Y_DOTS        = 51;   // ряд точек (r=3, центр y=53 → y=50..56)
static const int Y_FOOT        = 57;   // текст подвала (y=56..62, desc y=63)

// Вертикальная граница между половинами экрана (только в синей зоне).
static const int DIV_Y0 = 18;
static const int DIV_Y1 = 56;

void header(Display& d, const char* left, const char* right) {
  // title живёт в жёлтой зоне (y=0..7 при size=1, начинаем с y=1)
  d.text(0, 1, left, 1);
  if (right && right[0]) {
    int w = Display::textWidth(right, 1);
    d.text(OLED_WIDTH - w, 1, right, 1);
  }
  d.gfx().drawFastHLine(0, Y_RULE_TOP, OLED_WIDTH, SSD1306_WHITE);
}

void footer(Display& d, const char* text) {
  // Без линии-разделителя: она бы съела пиксели у подвала.
  // Достаточно того, что bar/dots выше уже рисуют границу.
  if (text && text[0]) d.textCentered(Y_FOOT, text, 1);
}

void divider(Display& d) {
  // только в синей зоне, чтобы не пересекать мёртвую полосу y=16..17
  d.gfx().drawFastVLine(SPLIT_X, DIV_Y0, DIV_Y1 - DIV_Y0 + 1, SSD1306_WHITE);
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
    // подсвеченное имя — белая плашка с инверсным текстом (строго в синей зоне)
    d.gfx().fillRect(x0, Y_NAME, x1 - x0 + 1, 8, SSD1306_WHITE);
    d.gfx().setTextColor(SSD1306_BLACK);
    d.textCenteredIn(x0, x1, Y_NAME, p.name, 1);
    d.gfx().setTextColor(SSD1306_WHITE);
  } else {
    d.textCenteredIn(x0, x1, Y_NAME, p.name, 1);
  }

  d.textCenteredIn(x0, x1, Y_BIG, p.big, 2);

  if (p.sub && p.sub[0]) d.textCenteredIn(x0, x1, Y_SUB, p.sub, 1);

  if (p.dots >= 0 && p.dotsMax > 0) {
    dotsRow(d, cx, Y_DOTS, p.dotsMax, p.dots);
  } else if (p.bar >= 0) {
    bar(d, x0 + 6, Y_BAR, (x1 - x0) - 11, 6, p.bar);
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
  // size=3 (21px) идёт в верхнюю часть синей зоны (y=18..38),
  // size=2 (14px) занимает y=24..37, помещается под sub.
  uint8_t size = (Display::textWidth(big, 3) <= OLED_WIDTH - 4) ? 3 : 2;
  d.textCentered(size == 3 ? 18 : Y_BIG, big, size);
  if (line1 && line1[0]) d.textCentered(Y_SUB, line1, 1);
  if (line2 && line2[0]) d.textCentered(Y_FOOT, line2, 1);
  footer(d, foot);
  d.flush();
}

void banner(Display& d, const char* big, const char* sub, const char* foot) {
  d.clear();
  // рамка (только в синей зоне, чтобы не упираться в мёртвую полосу)
  d.gfx().drawRect(0, 18, OLED_WIDTH, 46, SSD1306_WHITE);
  d.gfx().drawRect(2, 20, OLED_WIDTH - 4, 42, SSD1306_WHITE);
  uint8_t size = (Display::textWidth(big, 3) <= OLED_WIDTH - 10) ? 3 : 2;
  d.textCentered(size == 3 ? 18 : Y_BIG, big, size);
  if (sub && sub[0]) d.textCentered(Y_SUB, sub, 1);
  if (foot && foot[0]) d.textCentered(Y_FOOT, foot, 1);
  d.flush();
}

void menuFrame(Display& d, const char* title, const char* const* items,
               const uint8_t* playerCounts, int count, int sel, const char* foot) {
  d.clear();
  char right[10];
  snprintf(right, sizeof right, "%d/%d", sel + 1, count);
  header(d, title, right);

  // окно на 4 строки, выбранный пункт держим внутри.
  // строки начинаем с y=18 в синей зоне, шаг 9 (size=1 глиф=7 + 2 зазор)
  // → 18, 27, 36, 45 — все до линии подвала Y_RULE_BOT=54.
  const int rows = 4;
  const int ROW_Y0 = 18;
  const int ROW_DY = 9;
  int top = sel - rows / 2;
  if (top > count - rows) top = count - rows;
  if (top < 0) top = 0;

  for (int r = 0; r < rows && (top + r) < count; r++) {
    int idx = top + r;
    int y = ROW_Y0 + r * ROW_DY;
    bool cur = (idx == sel);
    // плашка выбранной строки строго в синей зоне (y=18..56).
    // текст size=1 глиф y+0..y+6, descender y+7; плашка = 8px начиная с y.
    if (cur) d.gfx().fillRect(0, y, OLED_WIDTH, 8, SSD1306_WHITE);
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
    // заголовок в жёлтой зоне (size=1 глиф y=1..7, помещается)
    d.textCentered(1, "LED ARCADE", 1);
    d.textCentered(Y_BIG, "АРКАДА", 2);
    bar(d, 14, Y_BAR, OLED_WIDTH - 28, 6, progress);
  } else if (phase == 1) {
    d.textCentered(Y_BIG, "ГОТОВ", 2);
    d.textCentered(Y_SUB, "32 ПИКСЕЛЯ ВЕСЕЛЬЯ", 1);
    // точки по самому низу (r=2), центр y=Y_FOOT+3 → y=Y_FOOT+1..Y_FOOT+5
    for (uint8_t i = 0; i < 8; i++)
      d.gfx().fillCircle(8 + i * 16, Y_FOOT + 3, 2, SSD1306_WHITE);
  } else {
    // заголовок в жёлтой зоне, всё остальное — в синей
    d.textCentered(1, "УПРАВЛЕНИЕ", 1);
    d.gfx().drawFastHLine(0, Y_RULE_TOP, OLED_WIDTH, SSD1306_WHITE);
    d.textCentered(Y_NAME, "ЗЕЛ=ВНИЗ", 1);
    d.textCentered(Y_BIG,  "СИН=ВВЕРХ", 2);
    d.textCentered(Y_FOOT, "МЕНЮ=ВЫБОР  УДЕРЖ=ТЕСТ", 1);
  }
  d.flushNow();
}

}  // namespace Ui

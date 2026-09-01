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

static const int SPLIT_X       = 63;   // колонка-разделитель половин

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

// ============================================================
//  Сплит-экран игр на двоих: каждая половина повёрнута к своему
//  игроку. Тот, кто сидит слева, читает своё табло повёрнутым по
//  часовой стрелке, тот, кто справа — против часовой.
//
//  Жёлтая полоса (y=0..15) остаётся общей и неповёрнутой: там имя
//  игры, номер раунда и строка события — их читают оба, и она всё
//  равно слишком узкая для повёрнутого текста.
//
//  Повороты делает сам Adafruit_GFX: при setRotation(1)/(3) экран
//  для рисования становится 64 (x) на 128 (y), а пиксель ложится
//  на физический так:
//      rot=1: (px, py) = (127 - y, x)      читается СЛЕВА
//      rot=3: (px, py) = (y, 63 - x)       читается СПРАВА
//  Отсюда обе панели живут в y=65..127 (это физические половины по
//  краям от разделителя), а синяя зона по x — это 18..63 при rot=1
//  и 0..45 при rot=3. В обоих случаях строки идут вниз по +y с
//  точки зрения своего игрока — раскладка панели общая.
// ============================================================

static const int RP_Y0   = 65;   // край панели у центра экрана (верх для игрока)
static const int RP_W    = 46;   // ширина синей зоны панели
static const int RP_NAME = 3;    // имя игрока
static const int RP_BIG  = 13;   // крупное значение
static const int RP_SUB  = 37;   // подпись
static const int RP_BAR  = 48;   // полоска / ряд точек

static void drawPanelRot(Display& d, uint8_t rot, const Panel& p) {
  d.gfx().setRotation(rot);

  const int x0 = (rot == 1) ? 18 : 0;      // синяя зона в повёрнутых координатах
  const int x1 = x0 + RP_W - 1;
  const int cx = (x0 + x1) / 2;
  const int y  = RP_Y0;

  if (p.active) {
    d.gfx().fillRect(x0, y + RP_NAME, RP_W, 8, SSD1306_WHITE);
    d.gfx().setTextColor(SSD1306_BLACK);
    d.textCenteredIn(x0, x1, y + RP_NAME, p.name, 1);
    d.gfx().setTextColor(SSD1306_WHITE);
  } else {
    d.textCenteredIn(x0, x1, y + RP_NAME, p.name, 1);
  }

  // крупное значение занимает всю ширину панели, сколько влезет
  uint8_t size = 1;
  if (Display::textWidth(p.big, 3) <= RP_W)      size = 3;
  else if (Display::textWidth(p.big, 2) <= RP_W) size = 2;
  d.textCenteredIn(x0, x1, y + RP_BIG, p.big, size);

  if (p.sub && p.sub[0]) d.textCenteredIn(x0, x1, y + RP_SUB, p.sub, 1);

  if (p.dots >= 0 && p.dotsMax > 0) {
    dotsRow(d, cx, y + RP_BAR + 3, p.dotsMax, p.dots);
  } else if (p.bar >= 0) {
    bar(d, x0 + 3, y + RP_BAR, RP_W - 6, 6, p.bar);
  }

  d.gfx().setRotation(0);
}

void split(Display& d, const char* title, const char* right,
           const Panel& left, const Panel& rightPanel, const char* foot) {
  d.clear();

  // общая жёлтая шапка: строка игры и строка события
  d.text(0, 0, title, 1);
  if (right && right[0]) {
    int w = Display::textWidth(right, 1);
    d.text(OLED_WIDTH - w, 0, right, 1);
  }
  if (foot && foot[0]) d.textCentered(8, foot, 1);

  // разделитель половин — только в синей зоне
  d.gfx().drawFastVLine(SPLIT_X, DIV_Y0, DIV_Y1 - DIV_Y0 + 1, SSD1306_WHITE);

  drawPanelRot(d, 1, left);        // левая половина — для игрока слева
  drawPanelRot(d, 3, rightPanel);  // правая половина — для игрока справа

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
    uint8_t pc = playerCounts ? playerCounts[idx] : 2;
    if (pc == 3) snprintf(tag, sizeof tag, "КО");     // кооператив
    else         snprintf(tag, sizeof tag, "%dИ", pc);
    d.text(2, y, cur ? ">" : " ", 1);
    d.text(10, y, items[idx], 1);
    d.text(OLED_WIDTH - 14, y, tag, 1);
  }
  d.gfx().setTextColor(SSD1306_WHITE);

  footer(d, foot);
  d.flush();
}

void rulesFrame(Display& d, const char* title, const char* right,
                 const char* const* lines, uint8_t count, const char* foot) {
  d.clear();
  header(d, title, right);

  // те же 4 строки, что и в меню: y=18,27,36,45 (шаг 9, size=1)
  const int ROW_Y0 = 18;
  const int ROW_DY = 9;
  for (uint8_t i = 0; i < count && i < 4; i++) {
    d.textCentered(ROW_Y0 + i * ROW_DY, lines[i], 1);
  }

  footer(d, foot);
  d.flush();
}

void bootFrame(Display& d, uint8_t phase, uint8_t progress) {
  d.clear();
  if (phase == 0) {
    // заголовок в жёлтой зоне (size=1 глиф y=1..7, помещается)
    d.textCentered(1, "ДОБРО ПОЖАЛОВАТЬ", 1);
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

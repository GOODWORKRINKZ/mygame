#include "games.h"

// ============================================================
//  TOWER — башня (1 игрок)
// ============================================================
//  Классика игровых автоматов, свёрнутая в одну строку: блок
//  ездит по ленте, нажатие фиксирует этаж. Что вылезло за границы
//  предыдущего этажа — отваливается, и блок становится уже.
//  Промахнулся мимо этажа целиком — башня рухнула. Идеальное
//  совпадение даёт бонус и ничего не отрезает.

static const char* KEY_BEST = "tower";

void TowerGame::reset() {
  pos = 0;
  dir = 1;
  speed = TOWER_SPEED_START;
  width = TOWER_WIDTH_START;
  prevLo = 0;
  prevHi = LED_COUNT - 1;
  level = 1;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  lockUntil = 0;
}

void TowerGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(0, 40, 90), rgb(0, 10, 30));
    char sub[22];
    snprintf(sub, sizeof sub, "ЭТАЖЕЙ %d  ОЧКИ %d", level - 1, score);
    Ui::banner(c.dpy, "БАШНЯ УПАЛА", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- блок ездит между краями ----
  float maxPos = (float)(LED_COUNT - width);
  pos += dir * speed * c.dt;
  if (pos >= maxPos) { pos = maxPos; dir = -1; }
  if (pos <= 0.0f)   { pos = 0.0f;   dir =  1; }

  // ---- фиксация этажа ----
  if (in.p1.pressed || in.p2.pressed) {
    int lo = (int)(pos + 0.5f);
    int hi = lo + width - 1;
    int keepLo = (lo > prevLo) ? lo : prevLo;
    int keepHi = (hi < prevHi) ? hi : prevHi;

    if (keepLo > keepHi) {
      // мимо этажа целиком — башня рухнула
      for (int i = lo; i <= hi; i++)
        c.fx.spark((float)i, (float)random(-18, 18), rgb(0, 160, 255), 0.5f);
      c.out.sfx(Sfx::GameOver);
      c.fx.flash(rgb(120, 0, 0), 260);
      over = true;
      overUntil = c.now + 1200;
      newRecord = store.submit(KEY_BEST, score);
      best = store.best(KEY_BEST);
      return;
    }

    int keepW = keepHi - keepLo + 1;
    int cut = width - keepW;

    // отвалившиеся пиксели улетают искрами в свою сторону
    for (int i = lo; i < keepLo; i++)
      c.fx.spark((float)i, -14.0f, rgb(0, 120, 200), 0.45f);
    for (int i = keepHi + 1; i <= hi; i++)
      c.fx.spark((float)i, 14.0f, rgb(0, 120, 200), 0.45f);

    if (cut == 0) {
      score += 10;                       // бонус за идеальный этаж
      c.out.sfx(Sfx::LevelUp);
      c.fx.shock((float)(keepLo + keepHi) / 2.0f, rgb(255, 255, 255), 40.0f, 260);
    } else {
      c.out.tone((uint16_t)(500 + level * 30), 45);
    }
    c.out.buzz(25);

    score += keepW * 2 + level;
    level++;
    prevLo = keepLo;
    prevHi = keepHi;
    width = keepW;
    speed *= TOWER_SPEED_UP;
    lockUntil = c.now + 200;

    // блок начинает новый проход с ближнего края
    if (pos > (float)(LED_COUNT - width)) pos = (float)(LED_COUNT - width);
  }

  render(c);
  oled(c);
}

void TowerGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // предыдущий этаж — тусклая "площадка", по ней и целимся
  for (int i = prevLo; i <= prevHi; i++) L.set(i, rgb(24, 24, 30));

  // текущий блок, цвет меняется с высотой
  int lo = (int)(pos + 0.5f);
  Color col = colHsv((uint8_t)(140 + level * 11), 255, 255);
  for (int k = 0; k < width; k++) {
    int i = lo + k;
    if (i < 0 || i >= LED_COUNT) continue;
    L.set(i, col);
  }

  // короткая вспышка после фиксации
  if (c.now < lockUntil) {
    uint8_t b = (uint8_t)(((lockUntil - c.now) * 120) / 200);
    L.addAll(colScale(rgb(255, 255, 255), b));
  }
}

void TowerGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[26];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "ЭТАЖ %d  ШИРИНА %d", level, width);
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, "Э%d", level);
  snprintf(foot, sizeof foot, "%s", hint());

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

#include "games.h"
#include <math.h>

// ============================================================
//  CATCH — лови (1 игрок, вся матрица 4x4)
// ============================================================
//  С верхнего ряда матрицы по одной из четырёх колонок падают блоки.
//  Внизу — корзина шириной две клетки: ЗЕЛЁНАЯ двигает её вправо,
//  СИНЯЯ — влево (зелёная кнопка физически справа). Поймал — очко,
//  упустил — минус жизнь. Чем больше очков, тем быстрее падает и тем
//  чаще два блока летят почти одновременно.
//  Рукав зелёного показывает жизни, рукав синего — очки на фоне рекорда.

static const char* KEY_BEST = "catch";
static const int   W = MATRIX_SIZE;             // 4 колонки
static const float FLOOR_Y = (float)(MATRIX_SIZE - 1);   // ряд корзины

static inline int mcol(int x) { return MATRIX_COL0 + x; }

void CatchGame::reset() {
  for (auto& d : drops) d.active = false;
  basket = 1;
  speed = CATCH_SPEED_START;
  spawnIn = 1.0f;
  lives = CATCH_LIVES;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  flashUntil = 0;
  flashGood = false;
  lastCol = -1;
}

void CatchGame::spawn() {
  for (auto& d : drops) {
    if (d.active) continue;
    int col;
    do { col = (int)random(0, W); } while (col == lastCol && random(3) != 0);
    lastCol = col;
    d.active = true;
    d.col = (uint8_t)col;
    d.y = -0.9f;                     // появляется над верхним рядом и въезжает в него
    return;
  }
}

void CatchGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(120, 60, 0), rgb(40, 20, 0));
    char sub[22];
    snprintf(sub, sizeof sub, "ПОЙМАНО %d", score);
    Ui::banner(c.dpy, "УПУСТИЛ", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- корзина: зелёная вправо, синяя влево ----
  if (in.p1.pressed && basket < W - 2) { basket++; c.out.sfx(Sfx::Click); }
  if (in.p2.pressed && basket > 0)     { basket--; c.out.sfx(Sfx::Click); }

  // ---- спавн: чем больше очков, тем чаще; иногда сразу второй блок ----
  spawnIn -= c.dt;
  if (spawnIn <= 0) {
    spawn();
    float gap = 1.6f - score * 0.03f;
    if (gap < 0.55f) gap = 0.55f;
    spawnIn = gap * ((float)random(80, 125) / 100.0f);
    if (score >= 8 && random(100) < 30) spawnIn *= 0.45f;
  }

  // ---- падение и приём ----
  for (auto& d : drops) {
    if (!d.active) continue;
    d.y += speed * c.dt;
    if (d.y < FLOOR_Y) continue;

    d.active = false;
    bool caught = (d.col >= basket && d.col <= basket + 1);
    flashUntil = c.now + 220;
    flashGood = caught;
    if (caught) {
      score++;
      speed += CATCH_SPEED_UP;
      if (speed > CATCH_SPEED_MAX) speed = CATCH_SPEED_MAX;
      c.out.sfx(Sfx::Pickup);
      c.out.buzz(15);
      c.fx.spark(0.0f, 10.0f, rgb(255, 220, 0), 0.3f, &ARM_P2);
    } else {
      lives--;
      c.out.sfx(Sfx::Error);
      c.fx.flash(rgb(160, 0, 0), 200);
      if (lives <= 0) {
        over = true;
        overUntil = c.now + 1200;
        newRecord = store.submit(KEY_BEST, score);
        best = store.best(KEY_BEST);
        c.out.sfx(Sfx::GameOver);
        return;
      }
    }
  }

  render(c);
  oled(c);
}

void CatchGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- поле: едва заметная сетка, ряд корзины чуть светлее ----
  for (int y = 0; y < MATRIX_SIZE; y++)
    for (int x = 0; x < W; x++)
      L.setXY(mcol(x), y, (y == MATRIX_SIZE - 1) ? rgb(8, 8, 12) : rgb(3, 3, 5));

  // ---- корзина ----
  Color bc = rgb(230, 230, 255);
  if (c.now < flashUntil) bc = flashGood ? rgb(80, 255, 120) : rgb(255, 40, 40);
  L.setXY(mcol(basket),     MATRIX_SIZE - 1, bc);
  L.setXY(mcol(basket + 1), MATRIX_SIZE - 1, bc);

  // ---- блоки: цвет по колонке, чтобы два одновременно читались ----
  for (auto& d : drops) {
    if (!d.active) continue;
    Color col = colHsv((uint8_t)(d.col * 60 + 10), 255, 255);
    L.addAAXY((float)mcol(d.col), d.y, col);
  }

  // ---- рукав зелёного: жизни; рукав синего: очки на фоне рекорда ----
  for (int i = 0; i < ARM_P1.len; i++)
    L.set(ARM_P1, i, (i < lives) ? colScale(rgb(255, 40, 40), 140) : rgb(6, 2, 2));
  int target = best > score ? best : (score > 0 ? score : 1);
  float lit = (float)score / (float)target * ARM_P2.len;
  for (int i = 0; i < ARM_P2.len; i++) {
    float f = lit - i;
    uint8_t b = f >= 1.0f ? 150 : (f > 0 ? (uint8_t)(f * 150) : 0);
    L.set(ARM_P2, i, colScale(rgb(255, 200, 0), (uint8_t)(8 + b)));
  }
}

void CatchGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[26];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "СКОРОСТЬ %d.%d", (int)speed, (int)(speed * 10) % 10);
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, "x%d", lives);
  char hearts[8] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(foot, sizeof foot, "ЖИЗНИ %s", hearts);
  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

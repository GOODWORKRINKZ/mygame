#include "games.h"
#include <math.h>

// ============================================================
//  PONG 2D — понг на матрице 4x4 (2 игрока)
// ============================================================
//  Поле 4x4 слишком мало для плавного мяча: на двух пикселях не
//  понять, куда он летит. Поэтому мяч ходит ШАГАМИ по клеткам, строго
//  по диагонали, как бильярдный шар: каждый тик — ровно одна клетка,
//  за мячом остаётся след, а в колонке ракетки тускло горит клетка,
//  в которую он прилетит. Задача — успеть переключить ракетку на ту
//  половину, где эта клетка. Каждый отбив ускоряет тик.
//  Ракетка зелёного — правая колонка матрицы, синего — левая; каждая
//  закрывает два ряда из четырёх, кнопка переключает верх/низ.

static const int N = MATRIX_SIZE;              // 4
static inline int mcol(int x) { return MATRIX_COL0 + x; }

void Pong2dGame::reset() {
  m.reset();
  padTop[0] = padTop[1] = true;
  newRally(millis());
}

void Pong2dGame::newRally(uint32_t now) {
  x = random(2) ? 1 : 2;
  y = random(2) ? 1 : 2;
  dx = random(2) ? 1 : -1;
  dy = random(2) ? 1 : -1;
  px = x; py = y;
  stepMs = PONG2D_STEP_START;
  rally = 0;
  serveAt = now + 900;
  nextStep = 0;
  goalUntil = 0;
}

// Какие ряды закрывает ракетка p: верхняя половина — 0 и 1, нижняя — 2 и 3.
bool Pong2dGame::covers(int p, int row) const {
  return padTop[p] ? (row < N / 2) : (row >= N / 2);
}

// Куда мяч прилетит в колонку ракетки, если никто ничего не изменит:
// прогоняем шаги вперёд с отскоками от верха и низа. Возвращает ряд и
// пишет в *side, в чью колонку (0 зелёный справа, 1 синий слева).
int Pong2dGame::impactRow(int* side) const {
  int cx = x, cy = y, cdy = dy;
  for (int k = 0; k < 2 * N; k++) {
    int ny = cy + cdy;
    if (ny < 0 || ny >= N) { cdy = -cdy; ny = cy + cdy; }
    int nx = cx + dx;
    if (nx >= N) { *side = 0; return ny; }
    if (nx < 0)  { *side = 1; return ny; }
    cx = nx; cy = ny;
  }
  *side = (dx > 0) ? 0 : 1;
  return cy;
}

void Pong2dGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRally(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  // ---- ракетки: кнопка переключает половину ----
  if (in.p1.pressed) { padTop[0] = !padTop[0]; c.out.tone(880, 18); }
  if (in.p2.pressed) { padTop[1] = !padTop[1]; c.out.tone(660, 18); }

  // ---- подача ----
  if (serveAt) {
    if (c.now >= serveAt) {
      serveAt = 0;
      nextStep = c.now + stepMs;
      c.out.sfx(Sfx::Beep);
    }
    render(c);
    oled(c);
    return;
  }

  // ---- шаг мяча ----
  if (c.now >= nextStep) {
    nextStep = c.now + stepMs;
    px = x; py = y;

    int ny = y + dy;
    if (ny < 0 || ny >= N) { dy = -dy; ny = y + dy; c.out.sfx(Sfx::Bounce); }

    int nx = x + dx;
    if (nx >= N || nx < 0) {
      int p = (nx >= N) ? 0 : 1;              // чья колонка
      if (covers(p, ny)) {
        bounce(p, c);
        dx = -dx;
        nx = x + dx;
      } else {
        y = ny;                                // мяч "улетает" за ракетку
        goal(p == 0 ? 2 : 1, c);
        render(c);
        oled(c);
        return;
      }
    }
    x = nx; y = ny;
  }

  render(c);
  oled(c);
}

void Pong2dGame::bounce(int p, Ctx& c) {
  rally++;
  stepMs = (uint16_t)(stepMs * PONG2D_STEP_UP);
  if (stepMs < PONG2D_STEP_MIN) stepMs = PONG2D_STEP_MIN;

  // Чистые диагонали на 4x4 замыкаются в цикл: мяч бьёт в одни и те же
  // ряды, и можно ничего не делать. Поэтому ракетка "подкручивает":
  // угол сохраняется, меняется на противоположный или мяч летит прямо
  // по ряду. Точка прилёта каждый раз другая — смотри на белую подсказку.
  int roll = (int)random(100);
  if (roll < 40)      { /* угол тот же */ }
  else if (roll < 75) { dy = (dy == 0) ? (random(2) ? 1 : -1) : -dy; }
  else                { dy = 0; }
  if (dy == 0 && random(100) < 50 && stepMs < PONG2D_STEP_START) dy = (y < N / 2) ? 1 : -1;
  c.out.tone((uint16_t)(500 + rally * 40), 40);
  c.out.buzz(20);
  c.fx.spark(0.0f, 8.0f, p == 0 ? COL_P1 : COL_P2, 0.25f, p == 0 ? &ARM_P1 : &ARM_P2);
}

void Pong2dGame::goal(int scorer, Ctx& c) {
  goalUntil = c.now + 600;
  c.fx.flash(scorer == 1 ? COL_P1 : COL_P2, 220);
  c.fx.burst(2.0f, scorer == 1 ? COL_P1 : COL_P2, 8, 14.0f, scorer == 1 ? &ARM_P1 : &ARM_P2);
  m.winRound(scorer, c.now, c);
}

void Pong2dGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- поле ----
  for (int yy = 0; yy < N; yy++)
    for (int xx = 0; xx < N; xx++)
      L.setXY(mcol(xx), yy, rgb(3, 3, 5));

  // ---- ракетки: две клетки своей колонки, другая половина едва видна ----
  for (int p = 0; p < 2; p++) {
    int cx = (p == 0) ? N - 1 : 0;
    int y0 = padTop[p] ? 0 : N / 2;
    int y1 = padTop[p] ? N / 2 : 0;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    L.setXY(mcol(cx), y0,     col);
    L.setXY(mcol(cx), y0 + 1, col);
    L.setXY(mcol(cx), y1,     colScale(col, 14));
    L.setXY(mcol(cx), y1 + 1, colScale(col, 14));
  }

  // ---- куда прилетит: тусклая белая клетка в колонке ракетки ----
  if (!serveAt) {
    int side;
    int row = impactRow(&side);
    int cx = (side == 0) ? N - 1 : 0;
    uint8_t pulse = (uint8_t)(70 + sin8((uint8_t)(c.now / 3)) / 4);
    L.addXY(mcol(cx), row, colScale(rgb(255, 255, 255), pulse));
  }

  // ---- мяч: след в прошлой клетке, сам мяч ярко ----
  if (serveAt) {
    uint8_t pulse = sin8((uint8_t)(c.now / 2));
    L.addXY(mcol(x), y, colScale(COL_BALL, (uint8_t)(60 + pulse / 2)));
  } else {
    // след гаснет по мере приближения следующего шага — видно направление
    uint32_t left = (nextStep > c.now) ? nextStep - c.now : 0;
    uint8_t trail = (uint8_t)(20 + (left * 90) / (stepMs ? stepMs : 1));
    L.addXY(mcol(px), py, colScale(COL_BALL, trail));
    L.addXY(mcol(x), y, COL_BALL);
  }

  // ---- рукава: счёт матча точками, темп — тусклая шкала ----
  for (int p = 0; p < 2; p++) {
    const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    float k = 1.0f - (float)(stepMs - PONG2D_STEP_MIN) / (float)(PONG2D_STEP_START - PONG2D_STEP_MIN);
    float lit = k * arm.len;
    for (int i = 0; i < arm.len; i++) {
      float f = lit - i;
      uint8_t b = f >= 1.0f ? 40 : (f > 0 ? (uint8_t)(f * 40) : 0);
      L.set(arm, i, colScale(col, (uint8_t)(6 + b)));
    }
    for (int k2 = 0; k2 < m.score[p] && k2 < ROUNDS_TO_WIN; k2++)
      L.set(arm, k2 * 2, colScale(col, 200));
    if (m.winner == p + 1 && c.now < goalUntil) L.fill(arm, colScale(col, 120));
  }
}

void Pong2dGame::oled(Ctx& c) {
  char b1[6], b2[6], foot[32];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  const char* s1 = padTop[0] ? "ВЕРХ" : "НИЗ";
  const char* s2 = padTop[1] ? "ВЕРХ" : "НИЗ";

  Panel l, r;
  // Зелёный (winner=1) — справа, синий (winner=2) — слева.
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (m.winner == 1);

  const char* f = foot;
  if (m.over())      f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner) f = "ГОЛ!";
  else if (serveAt)  f = "ПРИГОТОВЬСЯ...";
  else snprintf(foot, sizeof foot, "РОЗЫГРЫШ %d  ТИК %dМС", rally, stepMs);

  drawMatchOled(c, name(), m, l, r, f);
}

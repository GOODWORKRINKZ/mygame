#include "games.h"
#include <math.h>

// ============================================================
//  CHASE — догонялки по кольцу матрицы
// ============================================================
//  Раньше "кольцом" считалась вся лента, и переход с последнего
//  светодиода на нулевой был прыжком через весь стол. Теперь трасса —
//  настоящее кольцо: периметр матрицы 4x4 (12 клеток, RING). Один
//  убегает, второй догоняет, оба разгоняются нажатиями (скорость
//  гаснет, как в СПРИНТЕ), охотник чуть резвее. Фора — полкольца.
//  Рукава игроков показывают их текущую скорость, сердцевина —
//  сколько времени осталось у охотника. Роли меняются каждый раунд.

static const float RING_LEN = (float)RING.len;
static const float HALF     = RING_LEN / 2.0f;
static const float VEL_FULL = 11.0f;     // скорость, при которой шкала рукава полная

void ChaseGame::reset() {
  m.reset();
  newRound(millis());
}

void ChaseGame::newRound(uint32_t now) {
  hunter = (m.round % 2 == 1) ? 2 : 1;      // первый раунд догоняет синий
  int runner = 3 - hunter;

  // охотник стартует у своих ворот, беглец — у противоположных (фора полкольца)
  pos[hunter - 1] = (float)((hunter == 1) ? RING_GATE_P1 : RING_GATE_P2);
  pos[runner - 1] = RING.wrap(pos[hunter - 1] + HALF);
  vel[0] = vel[1] = 0.0f;

  startAt = now + 1400;
  endAt = startAt + CHASE_TIME_MS;
  gap = HALF;
}

void ChaseGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
      c.out.sfx(Sfx::Countdown);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  // ---- обратный отсчёт перед стартом ----
  if (startAt) {
    if (c.now >= startAt) startAt = 0;
    render(c);
    oled(c);
    return;
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    if (btn[p]->pressed) {
      float boost = (p + 1 == hunter) ? CHASE_IMPULSE * CHASE_HUNTER_BONUS
                                      : CHASE_IMPULSE;
      vel[p] += boost;
      c.out.tone((uint16_t)(480 + (int)(vel[p] * 26)), 14);
      c.fx.spark(pos[p], -6.0f, p == 0 ? COL_P1 : COL_P2, 0.18f, &RING);
    }
    vel[p] -= vel[p] * CHASE_FRICTION * c.dt;
    if (vel[p] < 0) vel[p] = 0;
    pos[p] = RING.wrap(pos[p] + vel[p] * c.dt);
  }

  int runner = 3 - hunter;
  gap = RING.ahead(pos[hunter - 1], pos[runner - 1]);

  // ---- поймал ----
  if (gap <= CHASE_CATCH_DIST || gap >= RING_LEN - CHASE_CATCH_DIST) {
    c.fx.burst(pos[hunter - 1], hunter == 1 ? COL_P1 : COL_P2, 14, 20.0f, &RING);
    c.out.buzz(90);
    m.winRound(hunter, c.now, c);
  } else if (c.now >= endAt) {
    // ---- время вышло, беглец ушёл ----
    c.fx.burst(pos[runner - 1], runner == 1 ? COL_P1 : COL_P2, 14, 20.0f, &RING);
    m.winRound(runner, c.now, c);
  }

  render(c);
  oled(c);
}

void ChaseGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.fade(120);                 // хвосты на кольце: видно, кто разогнался
  // рукава и сердцевина перерисовываются целиком, хвосты нужны только трассе
  L.fill(ARM_P1, 0);
  L.fill(ARM_P2, 0);

  int runner = 3 - hunter;
  bool close = (gap < 2.5f);

  // ---- трасса: тусклое кольцо ----
  for (int i = 0; i < RING.len; i++) L.add(RING, i, rgb(3, 3, 5));

  // ---- шкалы скорости в рукавах: от кнопки к матрице ----
  for (int p = 0; p < 2; p++) {
    const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    float lit = (vel[p] / VEL_FULL) * arm.len;
    for (int i = 0; i < arm.len; i++) {
      float f = lit - i;
      uint8_t b = f >= 1.0f ? 160 : (f > 0 ? (uint8_t)(f * 160) : 0);
      L.set(arm, i, colScale(col, (uint8_t)(10 + b)));
    }
    // у ворот — метка, чей это вход
    L.add(arm, arm.len - 1, colScale(col, 30));
  }

  // ---- сердцевина: остаток времени охотника, цвет охотника ----
  Color hc = (hunter == 1) ? COL_P1 : COL_P2;
  Color rc = (runner == 1) ? COL_P1 : COL_P2;
  if (!startAt) {
    uint32_t left = (endAt > c.now) ? endAt - c.now : 0;
    float k = (float)left / (float)CHASE_TIME_MS;              // 1 -> 0
    uint8_t rate = (uint8_t)(2 + (1.0f - k) * 8.0f);
    uint8_t pulse = sin8((uint8_t)(c.now * rate / 8));
    L.fill(CORE, colScale(hc, (uint8_t)(20 + (uint16_t)pulse * k * 120 / 255 + (1.0f - k) * 40)));
  }

  // ---- беглец ----
  L.addAA(RING, pos[runner - 1], rc);

  // ---- охотник: при сближении мигает белым ----
  if (close) {
    uint8_t b = ((c.now / 60) & 1) ? 255 : 120;
    L.addAA(RING, pos[hunter - 1], colScale(rgb(255, 255, 255), b));
  }
  L.addAA(RING, pos[hunter - 1], hc);
  L.addAA(RING, pos[hunter - 1] - 0.9f, colScale(hc, 60));

  // ---- обратный отсчёт: кольцо и сердцевина дышат белым ----
  if (startAt) {
    uint32_t left = (startAt > c.now) ? startAt - c.now : 0;
    uint8_t b = (uint8_t)(40 + (left % 400) * 120 / 400);
    for (int i = 0; i < RING.len; i++) L.add(RING, i, colScale(rgb(60, 60, 60), b));
    L.fill(CORE, colScale(rgb(60, 60, 60), b));
  }
}

void ChaseGame::oled(Ctx& c) {
  char b1[6], b2[6], foot[26];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  const char* s1 = (hunter == 1) ? "ОХОТНИК" : "БЕГЛЕЦ";
  const char* s2 = (hunter == 2) ? "ОХОТНИК" : "БЕГЛЕЦ";

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (hunter == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (hunter == 1);

  const char* f = foot;
  if (m.over()) {
    f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  } else if (m.winner) {
    f = (m.winner == hunter) ? "ПОЙМАЛ!" : "УШЕЛ!";
  } else if (startAt) {
    f = "ПРИГОТОВЬСЯ...";
  } else {
    uint32_t left = (endAt > c.now) ? (endAt - c.now) / 100 : 0;
    snprintf(foot, sizeof foot, "%lu.%lu СЕК  РАЗРЫВ %d",
             (unsigned long)(left / 10), (unsigned long)(left % 10), (int)(gap + 0.5f));
  }

  drawMatchOled(c, name(), m, l, r, f);
}

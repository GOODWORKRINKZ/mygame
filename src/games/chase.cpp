#include "games.h"
#include <math.h>

// ============================================================
//  CHASE — догонялки по кольцу
// ============================================================
//  Единственная игра, где лента считается замкнутой: с последнего
//  светодиода сразу попадаешь на нулевой. Один игрок убегает,
//  второй догоняет, оба разгоняются нажатиями (скорость гаснет,
//  как в СПРИНТЕ), но охотник чуть резвее. Не догнал за отведённое
//  время — очко беглецу. Роли меняются каждый раунд, поэтому матч
//  честный: побегать охотником успеют оба.

static inline float ringWrap(float p) {
  while (p >= (float)LED_COUNT) p -= (float)LED_COUNT;
  while (p < 0.0f)              p += (float)LED_COUNT;
  return p;
}

// Расстояние "вперёд по кольцу" от a до b.
static inline float ringAhead(float a, float b) {
  return ringWrap(b - a);
}

void ChaseGame::reset() {
  m.reset();
  newRound(millis());
}

void ChaseGame::newRound(uint32_t now) {
  hunter = (m.round % 2 == 1) ? 2 : 1;      // первый раунд догоняет синий
  int runner = 3 - hunter;

  pos[hunter - 1] = 0.0f;
  pos[runner - 1] = LED_COUNT / 2.0f;       // фора ровно полкольца
  vel[0] = vel[1] = 0.0f;

  startAt = now + 1400;
  endAt = startAt + CHASE_TIME_MS;
  gap = LED_COUNT / 2.0f;
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
      c.fx.spark(pos[p], -6.0f, p == 0 ? COL_P1 : COL_P2, 0.18f);
    }
    vel[p] -= vel[p] * CHASE_FRICTION * c.dt;
    if (vel[p] < 0) vel[p] = 0;
    pos[p] = ringWrap(pos[p] + vel[p] * c.dt);
  }

  int runner = 3 - hunter;
  gap = ringAhead(pos[hunter - 1], pos[runner - 1]);

  // ---- поймал ----
  if (gap <= CHASE_CATCH_DIST || gap >= LED_COUNT - CHASE_CATCH_DIST) {
    c.fx.burst(pos[hunter - 1], hunter == 1 ? COL_P1 : COL_P2, 14, 28.0f);
    c.out.buzz(90);
    m.winRound(hunter, c.now, c);
  } else if (c.now >= endAt) {
    // ---- время вышло, беглец ушёл ----
    c.fx.burst(pos[runner - 1], runner == 1 ? COL_P1 : COL_P2, 14, 28.0f);
    m.winRound(runner, c.now, c);
  }

  render(c);
  oled(c);
}

void ChaseGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.fade(120);                 // хвосты: по ним видно, кто разогнался

  int runner = 3 - hunter;
  bool close = (gap < 4.0f);

  // тусклое кольцо-дорожка
  for (int i = 0; i < LED_COUNT; i++) L.add(i, rgb(3, 3, 5));

  // беглец
  Color rc = (runner == 1) ? COL_P1 : COL_P2;
  L.addAA(pos[runner - 1], rc);

  // охотник: при сближении начинает мигать — обоим слышно и видно
  Color hc = (hunter == 1) ? COL_P1 : COL_P2;
  if (close) {
    uint8_t b = ((c.now / 60) & 1) ? 255 : 120;
    L.addAA(pos[hunter - 1], colScale(rgb(255, 255, 255), b));
  }
  L.addAA(pos[hunter - 1], hc);
  L.addAA(pos[hunter - 1] - 0.9f, colScale(hc, 60));

  // обратный отсчёт: кольцо дышит белым
  if (startAt) {
    uint32_t left = (startAt > c.now) ? startAt - c.now : 0;
    uint8_t b = (uint8_t)(40 + (left % 400) * 120 / 400);
    L.addAll(colScale(rgb(60, 60, 60), b));
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
             (unsigned long)(left / 10), (unsigned long)(left % 10), (int)gap);
  }

  drawMatchOled(c, name(), m, l, r, f);
}

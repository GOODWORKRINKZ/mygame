#include "games.h"
#include <math.h>

// ============================================================
//  SPRINT — гонка по ленте
// ============================================================
//  Зелёный бежит слева направо, синий справа налево. Каждое
//  нажатие даёт импульс, скорость постоянно гаснет — то есть
//  выигрывает не тот, кто сильнее жмёт, а тот, кто держит ритм.
//  Бегуны свободно проходят друг сквозь друга.

void SprintGame::reset() {
  m.reset();
  newRound(millis());
}

void SprintGame::newRound(uint32_t now) {
  pos[0] = 0;
  pos[1] = LED_COUNT - 1.0f;
  vel[0] = vel[1] = 0;
  startAt = now + 1400;    // обратный отсчёт
}

void SprintGame::update(const Inputs& in, Ctx& c) {
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

  // ---- обратный отсчёт ----
  if (startAt) {
    if (c.now >= startAt) startAt = 0;
    // фальстарт не наказываем, просто игнорируем нажатия
    render(c);
    oled(c);
    return;
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    if (btn[p]->pressed) {
      vel[p] += SPRINT_IMPULSE;
      c.out.tone((uint16_t)(520 + (int)(vel[p] * 25)), 14);
      c.fx.spark(pos[p], (p == 0 ? -1.0f : 1.0f) * 6.0f,
                 p == 0 ? COL_P1 : COL_P2, 0.2f);
    }
    vel[p] -= vel[p] * SPRINT_FRICTION * c.dt;
    if (vel[p] < 0) vel[p] = 0;
    pos[p] += (p == 0 ? 1.0f : -1.0f) * vel[p] * c.dt;
  }

  if (pos[0] >= LED_COUNT - 1.0f) {
    pos[0] = LED_COUNT - 1.0f;
    c.fx.burst(pos[0], COL_P1, 12, 24.0f);
    m.winRound(1, c.now, c);
  } else if (pos[1] <= 0.0f) {
    pos[1] = 0;
    c.fx.burst(pos[1], COL_P2, 12, 24.0f);
    m.winRound(2, c.now, c);
  }

  render(c);
  oled(c);
}

void SprintGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.fade(120);            // смазанный след — гонка сразу выглядит быстрой

  if (startAt) {
    // 3 - 2 - 1: столько ярких пикселей в центре
    uint32_t left = startAt - c.now;
    int n = (int)(left / 450) + 1;
    if (n > 3) n = 3;
    L.clear();
    for (int k = 0; k < n; k++) {
      L.set(LED_COUNT / 2 - 2 + k * 2, rgb(255, 200, 0));
    }
  }

  // финишные полосы: у каждого своя на противоположном краю
  uint8_t pulse = sin8((uint8_t)(c.now / 4));
  L.add(LED_COUNT - 1, colScale(COL_P1, (uint8_t)(30 + pulse / 4)));
  L.add(0,             colScale(COL_P2, (uint8_t)(30 + pulse / 4)));

  for (int p = 0; p < 2; p++) {
    Color col = (p == 0) ? COL_P1 : COL_P2;
    L.addAA(pos[p], col);
    // хвост длиной со скорость — визуальный спидометр
    float tail = vel[p] * 0.06f;
    if (tail > 3.0f) tail = 3.0f;
    for (float t = 0.6f; t < tail; t += 0.6f)
      L.addAA(pos[p] + (p == 0 ? -t : t), colScale(col, (uint8_t)(110 - t * 28)));
  }
}

void SprintGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[10], s2[10], foot[20];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);

  int prog1 = (int)((pos[0] / (LED_COUNT - 1.0f)) * 100.0f);
  int prog2 = (int)(((LED_COUNT - 1.0f - pos[1]) / (LED_COUNT - 1.0f)) * 100.0f);
  snprintf(s1, sizeof s1, "%d%%", prog1);
  snprintf(s2, sizeof s2, "%d%%", prog2);

  Panel l, r;
  // Зелёный игрок (winner=1, p=1) — справа, синий (winner=2, p=0) — слева.
  l.name = "BLUE";  l.big = b2; l.sub = s2; l.bar = prog2; l.active = (m.winner == 2);
  r.name = "GREEN"; r.big = b1; r.sub = s1; r.bar = prog1; r.active = (m.winner == 1);

  const char* f;
  if (m.over())      f = (m.winner == 1) ? "GREEN WINS THE MATCH" : "BLUE WINS THE MATCH";
  else if (m.winner) f = "photo finish!";
  else if (startAt)  { snprintf(foot, sizeof foot, "ready... %lu",
                                (unsigned long)((startAt - c.now) / 450 + 1)); f = foot; }
  else               f = hint();

  drawMatchOled(c, name(), m, l, r, f);
}

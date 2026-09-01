#include "games.h"
#include <math.h>

// ============================================================
//  TUG OF WAR — перетягивание каната
// ============================================================
//  Узел каната стоит в центре. Каждое нажатие тянет его к себе.
//  Чем чаще жмёшь (интервал < TUG_COMBO_MS), тем выше комбо и
//  тем сильнее рывок — награда за настоящий "долбёж" по кнопке.
//  Цель — вытащить узел за границу средней зоны на своей стороне.

// Границы: чуть за пределами средней зоны.
static const float GOAL_P1 = MIDDLE_HI + 0.5f;   // узел ушёл вправо — победа P1
static const float GOAL_P2 = MIDDLE_LO - 0.5f;   // узел ушёл влево  — победа P2

void TugGame::reset() {
  m.reset();
  rope = LED_COUNT / 2.0f;
  lastPress[0] = lastPress[1] = 0;
  combo[0] = combo[1] = 1;
  heat[0] = heat[1] = 0;
}

void TugGame::update(const Inputs& in, Ctx& c) {
  // накал сторон гаснет сам по себе
  for (int i = 0; i < 2; i++) {
    heat[i] -= c.dt * 2.4f;
    if (heat[i] < 0) heat[i] = 0;
  }

  // ---- показ результата раунда/матча ----
  if (!m.playing()) {
    if (m.tick(c.now)) {
      rope = LED_COUNT / 2.0f;
      combo[0] = combo[1] = 1;
      heat[0] = heat[1] = 0;
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  // ---- ввод ----
  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    if (!btn[p]->pressed) continue;

    // комбо: успел нажать в пределах окна — множитель растёт
    if (c.now - lastPress[p] <= TUG_COMBO_MS) {
      if (combo[p] < TUG_COMBO_MAX) combo[p]++;
    } else {
      combo[p] = 1;
    }
    lastPress[p] = c.now;
    heat[p] = 1.0f;

    float mult = 1.0f + (combo[p] - 1) * 0.18f;
    rope += (p == 0 ? TUG_STEP : -TUG_STEP) * mult;

    // звук растёт вместе с комбо — слышно, что "разгоняешься"
    c.out.tone((uint16_t)(680 + combo[p] * 130), 18);
    c.fx.spark(rope, (p == 0 ? 1 : -1) * 9.0f,
               p == 0 ? COL_P1 : COL_P2, 0.22f);
  }

  if (rope < 0.5f) rope = 0.5f;
  if (rope > LED_COUNT - 1.5f) rope = LED_COUNT - 1.5f;

  // ---- проверка победы ----
  if (rope > GOAL_P1)      { c.fx.burst(rope, COL_P1, 10, 20.0f); m.winRound(1, c.now, c); }
  else if (rope < GOAL_P2) { c.fx.burst(rope, COL_P2, 10, 20.0f); m.winRound(2, c.now, c); }

  render(c);
  oled(c);
}

void TugGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  for (int i = 0; i < LED_COUNT; i++) {
    float d = fabsf((float)i - rope);
    // канат ярче у узла и тускнеет к краям
    float b = 0.16f + 0.84f / (1.0f + d * 0.42f);
    bool mine = ((float)i < rope);
    b *= 0.72f + 0.28f * heat[mine ? 0 : 1];
    L.set(i, colScale(mine ? COL_P1 : COL_P2, (uint8_t)(b * 255.0f)));
  }

  // границы средней зоны — те самые "ворота", за которые надо вытащить
  L.add(MIDDLE_LO - 1, rgb(70, 70, 70));
  L.add(MIDDLE_HI + 1, rgb(70, 70, 70));

  // сам узел
  L.addAA(rope, rgb(255, 255, 255));
}

void TugGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[14], s2[14];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  snprintf(s1, sizeof s1, "COMBO x%d", combo[0]);
  snprintf(s2, sizeof s2, "COMBO x%d", combo[1]);

  // полоски = насколько близко каждый к своей "победной" границе
  float span = GOAL_P1 - GOAL_P2;
  int prog1 = (int)(((rope - GOAL_P2) / span) * 100.0f);
  int prog2 = 100 - prog1;

  Panel l, r;
  // Зелёный игрок (p=1) теперь физически справа — выводим его в правой панели.
  // Синий игрок (p=0) физически слева — в левой панели.
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.bar = prog2; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.bar = prog1; r.active = (m.winner == 1);

  const char* foot = hint();
  if (m.over())              foot = (m.winner == 1) ? "GREEN WINS THE MATCH" : "BLUE WINS THE MATCH";
  else if (m.winner)         foot = (m.winner == 1) ? "green takes the round" : "blue takes the round";

  drawMatchOled(c, name(), m, l, r, foot);
}

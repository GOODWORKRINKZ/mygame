#include "games.h"
#include <stdlib.h>

// ============================================================
//  HUNT — клад
// ============================================================
//  Где-то на ленте зарыт один светодиод-клад. Ходят по очереди:
//  курсор бегает, тап — раскопка. В ответ лента и бузер говорят,
//  насколько близко: жара, тепло, мороз. Отметки прошлых раскопок
//  остаются гореть цветом своей "температуры" и видны обоим —
//  то есть подсказками соперника можно пользоваться. Выигрывает
//  тот, кто первым сообразит, где копать, а не тот, кто быстрее.

// Цвет "температуры" по расстоянию до клада.
static Color heatColor(int d) {
  if (d <= 1) return rgb(255, 0, 0);
  if (d <= 3) return rgb(255, 110, 0);
  if (d <= 6) return rgb(255, 210, 0);
  if (d <= 10) return rgb(0, 170, 190);
  return rgb(0, 40, 220);
}

static const char* heatWord(int d) {
  if (d < 0)  return "--";
  if (d == 0) return "КЛАД!";
  if (d <= 1) return "ЖАРА!";
  if (d <= 3) return "ГОРЯЧО";
  if (d <= 6) return "ТЕПЛО";
  if (d <= 10) return "ХОЛОДНО";
  return "МОРОЗ";
}

void HuntGame::reset() {
  m.reset();
  newRound(millis());
}

void HuntGame::newRound(uint32_t now) {
  treasure = (int)random(0, LED_COUNT);
  cursor = 0;
  dir = 1;
  speed = HUNT_SPEED_START;
  turn = (m.round % 2 == 1) ? 1 : 2;    // право первого хода чередуется
  markCount = 0;
  shots[0] = shots[1] = 0;
  lastDist[0] = lastDist[1] = -1;
  digUntil = 0;
  digPos = -1;
  event = "";
  (void)now;
}

void HuntGame::dig(Ctx& c) {
  int pos = (int)(cursor + 0.5f);
  if (pos < 0) pos = 0;
  if (pos >= LED_COUNT) pos = LED_COUNT - 1;

  int d = abs(pos - treasure);
  int p = turn - 1;
  shots[p]++;
  lastDist[p] = d;
  digPos = pos;
  digUntil = c.now + 600;

  if (d == 0) {
    c.fx.burst((float)pos, rgb(255, 220, 0), 16, 30.0f);
    c.fx.flash(rgb(255, 200, 0), 400);
    c.out.buzz(120);
    event = (turn == 1) ? "ЗЕЛЕНЫЙ НАШЕЛ КЛАД" : "СИНИЙ НАШЕЛ КЛАД";
    m.winRound(turn, c.now, c);
    return;
  }

  // запоминаем отметку (самую старую вытесняем)
  if (markCount >= HUNT_MAX_MARKS) {
    for (int i = 1; i < HUNT_MAX_MARKS; i++) marks[i - 1] = marks[i];
    markCount = HUNT_MAX_MARKS - 1;
  }
  marks[markCount].pos = (int8_t)pos;
  marks[markCount].dist = (uint8_t)d;
  marks[markCount].owner = (int8_t)turn;
  markCount++;

  // чем ближе, тем выше писк — второй канал той же подсказки
  uint16_t freq = (uint16_t)(280 + (LED_COUNT - d) * 55);
  c.out.tone(freq, 130);
  c.out.buzz(d <= 1 ? 60 : 20);
  speed *= HUNT_SPEED_UP;
  event = heatWord(d);
}

void HuntGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  // ---- пауза после раскопки: показываем результат, потом ход переходит ----
  if (digUntil) {
    if (c.now >= digUntil) {
      digUntil = 0;
      digPos = -1;
      turn = 3 - turn;
      c.out.sfx(Sfx::Click);
    }
    render(c);
    oled(c);
    return;
  }

  // ---- курсор ----
  cursor += dir * speed * c.dt;
  if (cursor >= LED_COUNT - 1.0f) { cursor = LED_COUNT - 1.0f; dir = -1; }
  if (cursor <= 0.0f)             { cursor = 0.0f;             dir =  1; }

  // ---- копает только тот, чей ход ----
  const BtnState& b = (turn == 1) ? in.p1 : in.p2;
  const BtnState& other = (turn == 1) ? in.p2 : in.p1;
  if (b.pressed) {
    dig(c);
  } else if (other.pressed) {
    c.out.sfx(Sfx::Error);       // не твой ход
  }

  render(c);
  oled(c);
}

void HuntGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- отметки прошлых раскопок ----
  for (uint8_t i = 0; i < markCount; i++) {
    Color col = heatColor(marks[i].dist);
    // свежие отметки ярче старых
    uint8_t age = (uint8_t)(60 + (uint16_t)i * 120 / (markCount ? markCount : 1));
    L.set(marks[i].pos, colScale(col, age));
    // тонкая метка владельца поверх
    L.add(marks[i].pos, colScale(marks[i].owner == 1 ? COL_P1 : COL_P2, 25));
  }

  // ---- результат текущей раскопки ----
  if (digUntil && digPos >= 0) {
    int d = lastDist[turn - 1];
    uint8_t b = ((c.now / 90) & 1) ? 255 : 120;
    L.set(digPos, colScale(heatColor(d), b));
    return;
  }

  // ---- курсор цвета того, чей ход ----
  Color tc = (turn == 1) ? COL_P1 : COL_P2;
  L.addAA(cursor - dir * 1.0f, colScale(tc, 40));
  L.addAA(cursor, colScale(tc, 255));
}

void HuntGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  snprintf(s1, sizeof s1, "%s", heatWord(lastDist[0]));
  snprintf(s2, sizeof s2, "%s", heatWord(lastDist[1]));

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (turn == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (turn == 1);

  char foot[26];
  const char* f;
  if (m.over()) {
    f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  } else if (m.winner) {
    f = event;
  } else {
    snprintf(foot, sizeof foot, "ХОД %s  КОПОК %d",
             turn == 1 ? "ЗЕЛЕНОГО" : "СИНЕГО", shots[turn - 1]);
    f = foot;
  }

  drawMatchOled(c, name(), m, l, r, f);
}

#include "games.h"

// ============================================================
//  GREED — жадность
// ============================================================
//  Держишь кнопку — от твоего края ползёт полоска, и очки за неё
//  растут быстрее длины (квадратично): каждый лишний пиксель
//  дороже предыдущего. Но где-то на полоске зарыта мина, и она у
//  каждого своя. Отпустил до неё — забрал банк, дожал — сгорело
//  всё. Соперник видит твою полоску: главное веселье — терпеть
//  дольше него, когда он уже отпустил.

// Очки за длину: квадратичная кривая, чтобы риск реально окупался.
static inline int greedPoints(float len) {
  return (int)(len * len * 1.5f);
}

void GreedGame::reset() {
  m.reset();
  newRound(millis());
}

void GreedGame::newRound(uint32_t now) {
  for (int p = 0; p < 2; p++) {
    len[p] = 0;
    bank[p] = 0;
    done[p] = false;
    bust[p] = false;
    mine[p] = (int)random(GREED_MINE_MIN, GREED_MAX_LEN + 1);
    nextTick[p] = 0;
  }
  roundEnd = now + GREED_ROUND_MS;
  event = "";
}

void GreedGame::finish(int p, bool exploded, Ctx& c) {
  done[p] = true;
  bust[p] = exploded;
  float edge = (p == 0) ? len[p] : LED_COUNT - 1.0f - len[p];

  if (exploded) {
    bank[p] = 0;
    c.out.sfx(Sfx::Explode);
    c.out.buzz(120);
    c.fx.burst(edge, rgb(255, 90, 0), 12, 26.0f);
    event = (p == 0) ? "ЗЕЛЕНЫЙ ПОДОРВАЛСЯ" : "СИНИЙ ПОДОРВАЛСЯ";
  } else {
    bank[p] = greedPoints(len[p]);
    c.out.sfx(Sfx::Pickup);
    c.fx.spark(edge, (p == 0 ? -1.0f : 1.0f) * 12.0f,
               p == 0 ? COL_P1 : COL_P2, 0.4f);
    event = (p == 0) ? "ЗЕЛЕНЫЙ В БАНКЕ" : "СИНИЙ В БАНКЕ";
  }
}

void GreedGame::resolveRound(Ctx& c) {
  if (bank[0] > bank[1])      m.winRound(1, c.now, c);
  else if (bank[1] > bank[0]) m.winRound(2, c.now, c);
  else {
    // ничья (чаще всего оба подорвались) — раунд просто переигрывается
    c.out.sfx(Sfx::Back);
    newRound(c.now);
    event = "НИЧЬЯ, ЕЩЕ РАЗ";
  }
}

void GreedGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };

  for (int p = 0; p < 2; p++) {
    if (done[p]) continue;

    if (btn[p]->held) {
      len[p] += GREED_RATE * c.dt;

      // тиканье учащается вместе с длиной — слышно, как ты зарываешься
      if (c.now >= nextTick[p]) {
        uint16_t gap = (uint16_t)(260 - len[p] * 15);
        if (gap < 70) gap = 70;
        nextTick[p] = c.now + gap;
        c.out.tone((uint16_t)(600 + len[p] * 70), 16);
      }

      if (len[p] >= (float)mine[p]) {
        len[p] = (float)mine[p];
        finish(p, true, c);
        continue;
      }
      if (len[p] > GREED_MAX_LEN) len[p] = GREED_MAX_LEN;
    } else if (btn[p]->released && len[p] > 0.0f) {
      finish(p, false, c);
    }
  }

  // оба закончили — коротко показываем результат и считаем раунд
  if (done[0] && done[1] && roundEnd > c.now + 1100) roundEnd = c.now + 1100;

  if (c.now >= roundEnd) {
    for (int p = 0; p < 2; p++) {
      if (!done[p]) finish(p, false, c);   // не решился — банк по факту
    }
    resolveRound(c);
  }

  render(c);
  oled(c);
}

void GreedGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  for (int p = 0; p < 2; p++) {
    Color col = (p == 0) ? COL_P1 : COL_P2;
    int n = (int)len[p];

    for (int k = 0; k < n; k++) {
      int i = (p == 0) ? k : LED_COUNT - 1 - k;
      // к концу полоски жарче: видно, что ставки растут
      uint8_t b = (uint8_t)(50 + (uint16_t)k * 160 / GREED_MAX_LEN);
      Color cc = bust[p] ? rgb(120, 20, 0)
                         : (done[p] ? colScale(col, 70) : colScale(col, b));
      L.set(i, cc);
    }

    // кончик полоски — белый, пока игрок ещё держит
    if (!done[p] && len[p] > 0.2f) {
      float tip = (p == 0) ? len[p] - 0.5f : LED_COUNT - 0.5f - len[p];
      L.addAA(tip, rgb(255, 255, 255));
    }
    if (bust[p]) {
      float tip = (p == 0) ? len[p] - 0.5f : LED_COUNT - 0.5f - len[p];
      uint8_t b = ((c.now / 80) & 1) ? 255 : 60;
      L.addAA(tip, colScale(rgb(255, 60, 0), b));
    }
  }

  // центр — нейтральная разделительная точка
  L.add(LED_COUNT / 2, rgb(10, 10, 12));
}

void GreedGame::oled(Ctx& c) {
  char b1[8], b2[8], s1[16], s2[16];
  int live1 = done[0] ? bank[0] : greedPoints(len[0]);
  int live2 = done[1] ? bank[1] : greedPoints(len[1]);
  snprintf(b1, sizeof b1, "%d", live1);
  snprintf(b2, sizeof b2, "%d", live2);
  for (int p = 0; p < 2; p++) {
    char* dst = (p == 0) ? s1 : s2;
    if (bust[p])      snprintf(dst, 16, "БУМ!");
    else if (done[p]) snprintf(dst, 16, "В БАНКЕ");
    else              snprintf(dst, 16, "ПОБЕД %d", m.score[p]);
  }

  int bar1 = (int)((len[0] / GREED_MAX_LEN) * 100.0f);
  int bar2 = (int)((len[1] / GREED_MAX_LEN) * 100.0f);

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.bar = bar2; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.bar = bar1; r.active = (m.winner == 1);

  const char* foot = hint();
  if (m.over())      foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner) foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ВЗЯЛ РАУНД" : "СИНИЙ ВЗЯЛ РАУНД";
  else if (event[0]) foot = event;

  drawMatchOled(c, name(), m, l, r, foot);
}

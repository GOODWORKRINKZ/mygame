#include "games.h"
#include <math.h>

// ============================================================
//  PONG — одномерный пинг-понг со "шпингалетом"
// ============================================================
//  Держишь кнопку — шпингалет оттягивается и копит энергию
//  (визуально: от твоего края наливается полоска). Отпустил —
//  он резко выстреливает вперёд на длину заряда и так же быстро
//  уходит обратно. Мяч отбивается ТОЛЬКО на выбросе: попал —
//  мяч улетает тем быстрее, чем больше был заряд. Не успел —
//  гол. Промахнулся зарядом — шпингалет надо копить заново.

void Paddle::reset() {
  phase = PaddlePhase::Idle;
  charge = 0;
  ext = 0;
  lastStrike = 0;
}

void Paddle::update(float dt, bool held, bool released) {
  switch (phase) {
    case PaddlePhase::Idle:
      if (held) { phase = PaddlePhase::Charging; charge = 0; ext = 0; }
      break;

    case PaddlePhase::Charging:
      charge += PONG_CHARGE_RATE * dt;
      if (charge > PONG_PADDLE_MAX) charge = PONG_PADDLE_MAX;
      if (released || !held) {
        lastStrike = charge;
        phase = PaddlePhase::Strike;
        ext = 0;
      }
      break;

    case PaddlePhase::Strike:
      ext += PONG_STRIKE_SPEED * dt;
      if (ext >= charge) { ext = charge; phase = PaddlePhase::Return; }
      break;

    case PaddlePhase::Return:
      ext -= PONG_RETURN_SPEED * dt;
      if (ext <= 0) { ext = 0; charge = 0; phase = PaddlePhase::Idle; }
      break;
  }
}

void PongGame::reset() {
  m.reset();
  newRally(millis());
  rally = 0;
}

void PongGame::newRally(uint32_t now) {
  pad[0].reset();
  pad[1].reset();
  ball = LED_COUNT / 2.0f;
  vel = 0;
  rally = 0;
  serveAt = now + 900;
  lastStep[0] = lastStep[1] = -1;
}

void PongGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRally(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };

  // ---- шпингалеты ----
  for (int p = 0; p < 2; p++) {
    PaddlePhase before = pad[p].phase;
    pad[p].update(c.dt, btn[p]->held, btn[p]->released);

    // звук: каждый накопленный пиксель энергии — на полтона выше
    if (pad[p].phase == PaddlePhase::Charging) {
      int step = (int)pad[p].charge;
      if (step != lastStep[p]) {
        lastStep[p] = step;
        c.out.tone((uint16_t)(360 + step * 115), 45);
      }
    } else {
      lastStep[p] = -1;
    }
    if (before == PaddlePhase::Charging && pad[p].phase == PaddlePhase::Strike) {
      c.out.sfx(Sfx::Twang);
      c.fx.jet(p == 0 ? 0.0f : LED_COUNT - 1.0f, p == 0 ? 1 : -1,
               p == 0 ? COL_P1 : COL_P2, 3, 24.0f);
    }
  }

  // ---- подача ----
  if (serveAt) {
    if (c.now >= serveAt) {
      serveAt = 0;
      vel = (random(2) ? 1.0f : -1.0f) * PONG_BALL_MIN;
      c.out.sfx(Sfx::Beep);
    }
    render(c);
    oled(c);
    return;
  }

  // ---- мяч ----
  ball += vel * c.dt;

  if (vel < 0) {
    if (pad[0].striking() && ball <= pad[0].ext) {
      float k = pad[0].lastStrike / PONG_PADDLE_MAX;
      float speed = PONG_BALL_MIN + (PONG_BALL_MAX - PONG_BALL_MIN) * k;
      rally++;
      vel = speed * (1.0f + rally * 0.015f);
      ball = pad[0].ext + 0.15f;
      c.out.tone((uint16_t)(500 + speed * 45), 40);
      c.fx.burst(ball, COL_P1, 4, 12.0f);
      c.out.buzz(25);
    } else if (ball < -0.5f) {
      c.fx.flash(COL_P2, 200);
      m.winRound(2, c.now, c);
    }
  } else if (vel > 0) {
    if (pad[1].striking() && ball >= (LED_COUNT - 1.0f) - pad[1].ext) {
      float k = pad[1].lastStrike / PONG_PADDLE_MAX;
      float speed = PONG_BALL_MIN + (PONG_BALL_MAX - PONG_BALL_MIN) * k;
      rally++;
      vel = -speed * (1.0f + rally * 0.015f);
      ball = (LED_COUNT - 1.0f) - pad[1].ext - 0.15f;
      c.out.tone((uint16_t)(500 + speed * 45), 40);
      c.fx.burst(ball, COL_P2, 4, 12.0f);
      c.out.buzz(25);
    } else if (ball > LED_COUNT - 0.5f) {
      c.fx.flash(COL_P1, 200);
      m.winRound(1, c.now, c);
    }
  }

  render(c);
  oled(c);
}

void PongGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  for (int p = 0; p < 2; p++) {
    Color base = (p == 0) ? COL_P1 : COL_P2;
    const Paddle& pd = pad[p];

    if (pd.phase == PaddlePhase::Charging) {
      // накопленная энергия: полоска от края, чем больше — тем ярче
      int n = (int)ceilf(pd.charge);
      for (int k = 0; k < n; k++) {
        int idx = (p == 0) ? k : (LED_COUNT - 1 - k);
        uint8_t b = (uint8_t)(40 + (180 * (k + 1)) / (n > 0 ? n : 1));
        L.set(idx, colScale(base, b));
      }
      if (pd.charge >= PONG_PADDLE_MAX - 0.05f) {
        // заряд полный — край мигает белым
        uint8_t pulse = sin8((uint8_t)(c.now / 2));
        L.set(p == 0 ? 0 : LED_COUNT - 1, colScale(rgb(255, 255, 255), pulse));
      }
    } else if (pd.ext > 0) {
      int n = (int)ceilf(pd.ext);
      bool hot = (pd.phase == PaddlePhase::Strike);
      for (int k = 0; k < n; k++) {
        int idx = (p == 0) ? k : (LED_COUNT - 1 - k);
        L.set(idx, hot ? colLerp(base, rgb(255, 255, 255), 90) : colScale(base, 60));
      }
      if (hot) L.addAA(p == 0 ? pd.ext : (LED_COUNT - 1.0f) - pd.ext, rgb(255, 255, 255));
    } else {
      L.set(p == 0 ? 0 : LED_COUNT - 1, colScale(base, 26));
    }
  }

  // мяч: шлейф позади + яркая голова
  if (serveAt) {
    uint8_t pulse = sin8((uint8_t)(c.now / 2));
    L.addAA(ball, colScale(rgb(255, 255, 255), (uint8_t)(60 + pulse / 2)));
  } else {
    float dirSign = (vel > 0) ? -1.0f : 1.0f;
    L.addAA(ball + dirSign * 1.0f, rgb(50, 50, 60));
    L.addAA(ball, rgb(255, 255, 255));
  }
}

void PongGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16], foot[32];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);

  for (int p = 0; p < 2; p++) {
    char* dst = (p == 0) ? s1 : s2;
    switch (pad[p].phase) {
      case PaddlePhase::Charging: snprintf(dst, 16, "ТЯНИ %d", (int)pad[p].charge); break;
      case PaddlePhase::Strike:   strcpy(dst, "УДАР!"); break;
      case PaddlePhase::Return:   strcpy(dst, "ВОЗВРАТ"); break;
      default:                    strcpy(dst, "ГОТОВ"); break;
    }
  }

  Panel l, r;
  int bar1 = (int)((pad[0].charge / PONG_PADDLE_MAX) * 100.0f);
  int bar2 = (int)((pad[1].charge / PONG_PADDLE_MAX) * 100.0f);
  l.name = "ЗЕЛЕНЫЙ"; l.big = b1; l.sub = s1; l.bar = bar1; l.active = (m.winner == 1);
  r.name = "СИНИЙ";   r.big = b2; r.sub = s2; r.bar = bar2; r.active = (m.winner == 2);

  const char* f = foot;
  if (m.over())        f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner)   f = "ГОЛ!";
  else if (serveAt)    f = "ПРИГОТОВЬСЯ...";
  else                 snprintf(foot, sizeof foot, "РОЗЫГРЫШ %d", rally);

  drawMatchOled(c, name(), m, l, r, f);
}

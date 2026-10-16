#include "games.h"

// ============================================================
//  TRAFFIC — стоп-сигнал
// ============================================================
//  Лента ждёт в тусклом покое и вдруг вспыхивает на TRAFFIC_WINDOW_MS:
//  зелёным — кто первый нажал, забрал очко; красным — кто нажал,
//  тот очко подарил. Дуэль проверяет скорость, а эта игра —
//  способность не дёрнуться. Нажатие до вспышки = фальстарт.

void TrafficGame::reset() {
  m.reset();
  arm(millis());
}

void TrafficGame::arm(uint32_t now) {
  ph = Ph::Arming;
  phaseUntil = now + 800;
  signalAt = 0;
  rt[0] = rt[1] = -1;
}

void TrafficGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      arm(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  switch (ph) {
    case Ph::Arming:
      if (c.now >= phaseUntil) {
        ph = Ph::Wait;
        signalAt = c.now + (uint32_t)random(TRAFFIC_WAIT_MIN, TRAFFIC_WAIT_MAX);
        redLight = (random(100) < TRAFFIC_RED_PERCENT);
      }
      break;

    case Ph::Wait: {
      // дёрнулся до сигнала — очко сопернику
      int early = in.p1.pressed ? 1 : (in.p2.pressed ? 2 : 0);
      if (early) {
        rt[early - 1] = -2;
        c.out.sfx(Sfx::Error);
        c.fx.flash(rgb(200, 0, 0), 320);
        m.winRound(early == 1 ? 2 : 1, c.now, c);
        break;
      }
      if (c.now >= signalAt) {
        ph = Ph::Signal;
        phaseUntil = c.now + TRAFFIC_WINDOW_MS;
        // звук одинаковый для обоих сигналов: решать надо глазами
        c.out.tone(1900, 70);
        c.out.buzz(40);
      }
      break;
    }

    case Ph::Signal: {
      int hit = in.p1.pressed ? 1 : (in.p2.pressed ? 2 : 0);
      if (hit) {
        int dt = (int)(c.now - signalAt);
        if (redLight) {
          // на красный жать нельзя — очко сопернику
          rt[hit - 1] = -2;
          c.out.sfx(Sfx::Error);
          c.fx.flash(rgb(220, 0, 0), 320);
          m.winRound(hit == 1 ? 2 : 1, c.now, c);
        } else {
          rt[hit - 1] = dt;
          c.fx.burst(hit == 1 ? 1.0f : LED_COUNT - 2.0f,
                     hit == 1 ? COL_P1 : COL_P2, 12, 26.0f);
          m.winRound(hit, c.now, c);
        }
        break;
      }
      if (c.now >= phaseUntil) {
        // окно закрылось: на красный это правильное поведение обоих
        if (redLight) {
          c.out.tone(1100, 60);
          c.fx.flash(rgb(0, 60, 0), 200);
        }
        arm(c.now);
      }
      break;
    }

    default: break;
  }

  render(c);
  oled(c);
}

void TrafficGame::render(Ctx& c) {
  LedStrip& L = c.leds;

  switch (ph) {
    case Ph::Arming: {
      uint8_t b = (uint8_t)(20 + (uint16_t)sin8((uint8_t)(c.now / 6)) * 70 / 255);
      L.fillAll(colScale(rgb(255, 170, 0), b));
      break;
    }

    case Ph::Wait:
      // ровный тусклый янтарь — по картинке цвет сигнала не угадать
      L.fillAll(rgb(24, 14, 0));
      break;

    case Ph::Signal: {
      uint32_t el = c.now - signalAt;
      uint8_t b = el > TRAFFIC_WINDOW_MS ? 40
                                         : (uint8_t)(255 - (el * 150) / TRAFFIC_WINDOW_MS);
      L.fillAll(colScale(redLight ? rgb(255, 0, 0) : rgb(0, 255, 40), b));
      break;
    }

    default:
      L.clear();
      break;
  }
}

static void trafficRt(char* dst, size_t n, int v) {
  if (v == -2)    snprintf(dst, n, "ОШИБКА");
  else if (v < 0) snprintf(dst, n, "--");
  else            snprintf(dst, n, "%dмс", v);
}

void TrafficGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  trafficRt(s1, sizeof s1, rt[0]);
  trafficRt(s2, sizeof s2, rt[1]);

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (m.winner == 1);

  const char* foot = hint();
  if (m.over())               foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner)          foot = (rt[m.winner - 1] == -2) ? "ЧУЖАЯ ОШИБКА" : "ЕСТЬ РЕАКЦИЯ!";
  else if (ph == Ph::Arming)  foot = "ПРИГОТОВЬСЯ...";
  else if (ph == Ph::Wait)    foot = "СМОТРИ НА ЛЕНТУ";
  else if (ph == Ph::Signal)  foot = redLight ? "КРАСНЫЙ! НЕ ЖМИ" : "ЗЕЛЕНЫЙ! ЖМИ";

  drawMatchOled(c, name(), m, l, r, foot);
}

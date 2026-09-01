#include "games.h"

// ============================================================
//  DUEL — дуэль на реакцию
// ============================================================
//  Лента наливается тревожным красным, потом в случайный момент
//  вспыхивает белым. Кто первый нажал после вспышки — забрал раунд,
//  на экране видно время реакции в миллисекундах. Нажал до вспышки —
//  фальстарт, раунд уходит сопернику.

void DuelGame::reset() {
  m.reset();
  arm(millis());
}

void DuelGame::arm(uint32_t now) {
  ph = Ph::Arming;
  phaseUntil = now + 900;
  goAt = 0;
  rt[0] = rt[1] = -1;
  falseStart = false;
}

void DuelGame::update(const Inputs& in, Ctx& c) {
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
        goAt = c.now + (uint32_t)random(DUEL_WAIT_MIN, DUEL_WAIT_MAX);
      }
      break;

    case Ph::Wait: {
      // фальстарт
      int early = 0;
      if (in.p1.pressed) early = 1;
      else if (in.p2.pressed) early = 2;
      if (early) {
        falseStart = true;
        rt[early - 1] = -2;
        c.out.sfx(Sfx::Error);
        c.fx.flash(rgb(200, 0, 0), 350);
        m.winRound(early == 1 ? 2 : 1, c.now, c);
        break;
      }
      if (c.now >= goAt) {
        ph = Ph::Go;
        c.out.tone(2200, 90);
        c.out.buzz(60);
        c.fx.flash(rgb(255, 255, 255), 160);
      }
      break;
    }

    case Ph::Go: {
      int winner = 0;
      if (in.p1.pressed) { rt[0] = (int)(c.now - goAt); winner = 1; }
      else if (in.p2.pressed) { rt[1] = (int)(c.now - goAt); winner = 2; }
      if (winner) {
        c.fx.burst(winner == 1 ? 1.0f : LED_COUNT - 2.0f,
                   winner == 1 ? COL_P1 : COL_P2, 12, 26.0f);
        m.winRound(winner, c.now, c);
      } else if (c.now - goAt > 4000) {
        // никто не нажал — перезапуск раунда без счёта
        arm(c.now);
      }
      break;
    }

    default: break;
  }

  render(c);
  oled(c);
}

void DuelGame::render(Ctx& c) {
  LedStrip& L = c.leds;

  switch (ph) {
    case Ph::Arming: {
      // "к барьеру": янтарное дыхание
      uint8_t b = (uint8_t)(30 + (uint16_t)sin8((uint8_t)(c.now / 6)) * 120 / 255);
      L.fillAll(colScale(rgb(255, 140, 0), b));
      break;
    }
    case Ph::Wait:
      // ровный тусклый красный: по картинке нельзя угадать момент вспышки
      L.fillAll(rgb(40, 0, 0));
      break;

    case Ph::Go: {
      // ослепительная вспышка, плавно гаснущая
      uint32_t el = c.now - goAt;
      uint8_t b = el > 600 ? 60 : (uint8_t)(255 - (el * 195) / 600);
      L.fillAll(colScale(rgb(255, 255, 255), b));
      break;
    }
    default:
      L.clear();
      break;
  }
}

static void rtText(char* dst, size_t n, int v) {
  if (v == -2)      snprintf(dst, n, "ФОЛС!");
  else if (v < 0)   snprintf(dst, n, "--");
  else              snprintf(dst, n, "%dмс", v);
}

void DuelGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  rtText(s1, sizeof s1, rt[0]);
  rtText(s2, sizeof s2, rt[1]);

  Panel l, r;
  // Зелёный (winner=1) — справа, синий (winner=2) — слева.
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (m.winner == 1);

  const char* foot = "ЖДИ ВСПЫШКУ!";
  if (m.over())          foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (falseStart)   foot = "ФАЛЬСТАРТ!";
  else if (m.winner)     foot = "ПОБЕДА ЗА СКОРОСТЬ";
  else if (ph == Ph::Arming) foot = "ПРИГОТОВЬСЯ...";
  else if (ph == Ph::Go) foot = "СТАРТ!";

  drawMatchOled(c, name(), m, l, r, foot);
}

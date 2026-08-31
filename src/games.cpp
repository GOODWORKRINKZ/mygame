#include "games.h"

// ============================== TugGame ==============================

void TugGame::reset() {
  marker = LED_COUNT / 2;
  score[0] = score[1] = 0;
  round = 1;
  winFlash = 0;
  over = 0;
  flashUntil = 0;
}

void TugGame::update(uint32_t dtMs, const Inputs& in,
                     LedStrip& leds, Display& dpy, Output& out) {
  (void)dtMs;
  uint32_t now = millis();

  if (winFlash && now >= flashUntil) {
    winFlash = 0;
    if (over) { over = 0; reset(); }
    else marker = LED_COUNT / 2;
  }

  if (!winFlash && !over) {
    if (in.p1Pressed) { marker++; out.click(); }
    if (in.p2Pressed) { marker--; out.click(); }
    if (marker < 0) marker = 0;
    if (marker > LED_COUNT - 1) marker = LED_COUNT - 1;

    if (marker > MIDDLE_HI) {           // игрок 1 вытолкнул соперника
      score[0]++;
      winFlash = 1;
      flashUntil = now + 1200;
      out.winSound();
      if (score[0] >= ROUNDS_TO_WIN) { over = 1; flashUntil = now + 3000; }
      else round++;
    } else if (marker < MIDDLE_LO) {    // игрок 2 вытолкнул соперника
      score[1]++;
      winFlash = 2;
      flashUntil = now + 1200;
      out.winSound();
      if (score[1] >= ROUNDS_TO_WIN) { over = 2; flashUntil = now + 3000; }
      else round++;
    }
  }

  render(leds);
  drawOLED(dpy);
}

void TugGame::render(LedStrip& leds) {
  leds.clear();

  if (winFlash) {
    leds.fillAll(winFlash == 1 ? leds.rgb(0, 255, 0) : leds.rgb(0, 0, 255));
    return;
  }

  for (int i = MIDDLE_LO; i <= MIDDLE_HI; i++)
    leds.setLogical(i, leds.rgb(26, 26, 26));          // средняя зона

  for (int i = 0; i < marker; i++)
    leds.setLogical(i, leds.rgb(20, 190, 20));         // змейка P1

  leds.setLogical(marker, leds.rgb(255, 255, 255));    // точка встречи

  for (int i = marker + 1; i < LED_COUNT; i++)
    leds.setLogical(i, leds.rgb(20, 20, 230));         // змейка P2
}

void TugGame::drawOLED(Display& dpy) {
  char l1[22], l2[24];
  if (over) {
    snprintf(l1, sizeof l1, "P%d WINS THE MATCH", over);
    l2[0] = 0;
  } else if (winFlash) {
    snprintf(l1, sizeof l1, "P%d WINS THE ROUND", winFlash);
    snprintf(l2, sizeof l2, "ROUND %d", round);
  } else {
    snprintf(l1, sizeof l1, "ROUND %d", round);
    snprintf(l2, sizeof l2, "pull the middle zone");
  }
  dpy.gameFrame("TUG OF WAR", score[0], score[1], l1, l2);
}

// ============================== ShooterGame ==============================

void ShooterGame::reset() {
  hp[0] = hp[1] = SHOOTER_HP;
  score[0] = score[1] = 0;
  round = 1;
  over = 0;
  p1x = p2x = -1;
  cd[0] = cd[1] = 0;
  lastMove = 0;
  explode = false;
  explodeAt = 0;
  explodeUntil = 0;
  overUntil = 0;
}

void ShooterGame::update(uint32_t dtMs, const Inputs& in,
                         LedStrip& leds, Display& dpy, Output& out) {
  (void)dtMs;
  uint32_t now = millis();

  // Выполняем отложенные эффекты, запланированные из private-методов,
  // в которых объекта Output под рукой нет (у нас друзья не объявлены).
  if (_fxHit)     { out.buzz(200); out.tone(392, 80); _fxHit = false; }
  if (_fxWin)     { out.winSound();              _fxWin = false; }

  if (over) {
    if (now >= overUntil) reset();
    render(leds);
    drawOLED(dpy);
    return;
  }

  // выстрелы (с перезарядкой)
  if (in.p1Pressed && p1x < 0 && now >= cd[0]) {
    p1x = hp[0];                       // стартует сразу за базой P1
    cd[0] = now + SHOT_COOLDOWN_MS;
    out.beep();
  }
  if (in.p2Pressed && p2x < 0 && now >= cd[1]) {
    p2x = LED_COUNT - 1 - hp[1];       // стартует сразу за базой P2
    cd[1] = now + SHOT_COOLDOWN_MS;
    out.beep();
  }

  // движение снарядов
  if (now - lastMove >= SHOT_SPEED_MS) {
    lastMove = now;
    if (p1x >= 0) p1x++;
    if (p2x >= 0) p2x--;

    if (p1x >= 0 && p2x >= 0 && p1x >= p2x) {
      // лобовое столкновение — оба снаряда уничтожены
      explodeAt = (p1x + p2x) / 2;
      explode = true;
      explodeUntil = now + 250;
      p1x = p2x = -1;
      out.click();
      out.buzz(120);
    } else if (p1x >= 0 && p1x >= LED_COUNT - hp[1]) {
      hitBase(1);                       // снаряд P1 долетел до базы P2
    } else if (p2x >= 0 && p2x <= hp[0] - 1) {
      hitBase(2);                       // снаряд P2 долетел до базы P1
    }
  }

  if (explode && now >= explodeUntil) explode = false;

  render(leds);
  drawOLED(dpy);
}

void ShooterGame::hitBase(int attacker) {
  int defender = (attacker == 1) ? 1 : 0;   // индекс в hp[]
  hp[defender]--;
  explodeAt = (attacker == 1) ? LED_COUNT - 1 : 0;
  explode = true;
  explodeUntil = millis() + 300;
  if (attacker == 1) p1x = -1; else p2x = -1;
  hitFx();   // звук/вибрация выносятся в update()
  if (hp[defender] <= 0) roundWin(attacker);
}

void ShooterGame::roundWin(int p) {
  score[p - 1]++;
  if (score[p - 1] >= ROUNDS_TO_WIN) {
    over = p;
    overUntil = millis() + 3000;
  } else {
    round++;
  }
  p1x = p2x = -1;
  hp[0] = hp[1] = SHOOTER_HP;
  winFx();   // звук победителя — в update()
}

void ShooterGame::render(LedStrip& leds) {
  leds.clear();

  for (int i = 0; i < hp[0]; i++)
    leds.setLogical(i, leds.rgb(20, 200, 20));                // база P1

  for (int i = 0; i < hp[1]; i++)
    leds.setLogical(LED_COUNT - 1 - i, leds.rgb(20, 20, 220)); // база P2

  if (p1x >= 0) leds.setLogical(p1x, leds.rgb(255, 255, 255));
  if (p2x >= 0) leds.setLogical(p2x, leds.rgb(255, 255, 255));

  if (explode) {
    uint32_t r = leds.rgb(255, 0, 0);
    for (int i = explodeAt - 2; i <= explodeAt + 2; i++)
      if (i >= 0 && i < LED_COUNT) leds.setLogical(i, r);
  }
}

void ShooterGame::drawOLED(Display& dpy) {
  char l1[22], l2[24];
  if (over) {
    snprintf(l1, sizeof l1, "P%d WINS THE MATCH", over);
    l2[0] = 0;
  } else {
    snprintf(l1, sizeof l1, "ROUND %d", round);
    snprintf(l2, sizeof l2, "HP %d : %d", hp[0], hp[1]);
  }
  dpy.gameFrame("SHOOTER", score[0], score[1], l1, l2);
}

// ============================== PongGame ==============================

void PongGame::reset() {
  p1 = Paddle();
  p2 = Paddle();
  ball = LED_COUNT / 2;
  dir = (random(2) == 0) ? -1 : 1;
  score[0] = score[1] = 0;
  round = 1;
  over = 0;
  lastTick = 0;
  overUntil = 0;
}

void Paddle::tick(bool held) {
  switch (phase) {
    case PaddlePhase::Idle:
      if (held) { phase = PaddlePhase::Charging; charge = 0; }
      break;
    case PaddlePhase::Charging:
      if (held) { if (charge < PONG_PADDLE_MAX) charge++; }
      else { phase = PaddlePhase::Strike; ext = 0; }
      break;
    case PaddlePhase::Strike:
      ext += 2;
      if (ext >= charge) { ext = charge; phase = PaddlePhase::Return; }
      break;
    case PaddlePhase::Return:
      ext -= 2;
      if (ext <= 0) { ext = 0; phase = PaddlePhase::Idle; }
      break;
  }
}

void PongGame::update(uint32_t dtMs, const Inputs& in,
                      LedStrip& leds, Display& dpy, Output& out) {
  (void)dtMs;
  uint32_t now = millis();

  if (_fxWin)  { out.winSound();  _fxWin  = false; }
  if (_fxLose) { out.loseSound(); _fxLose = false; }

  if (over) {
    if (now >= overUntil) reset();
    render(leds);
    drawOLED(dpy);
    return;
  }

  if (now - lastTick >= PONG_TICK_MS) {
    lastTick = now;
    p1.tick(in.p1Held);
    p2.tick(in.p2Held);

    if (dir < 0) {
      // мяч летит к игроку 1 (влево)
      if (p1.blocking() && ball <= p1.ext) {
        dir = 1; ball += 1; out.click();       // отбит
      } else if (ball <= 0) {
        goal(2);                               // гол игроку 2
      } else {
        ball -= 1;
      }
    } else {
      // мяч летит к игроку 2 (вправо)
      if (p2.blocking() && ball >= LED_COUNT - 1 - p2.ext) {
        dir = -1; ball -= 1; out.click();      // отбит
      } else if (ball >= LED_COUNT - 1) {
        goal(1);                               // гол игроку 1
      } else {
        ball += 1;
      }
    }
  }

  render(leds);
  drawOLED(dpy);
}

void PongGame::goal(int scorer) {
  score[scorer - 1]++;
  if (score[scorer - 1] >= ROUNDS_TO_WIN) {
    over = scorer;
    overUntil = millis() + 3000;
    queueWin();
  } else {
    round++;
    queueLose();
  }
  p1 = Paddle();
  p2 = Paddle();
  ball = LED_COUNT / 2;
  dir = (random(2) == 0) ? -1 : 1;
}

void PongGame::render(LedStrip& leds) {
  leds.clear();

  leds.setLogical(ball, leds.rgb(255, 255, 255));   // мяч

  // шпингалет P1 (зелёный)
  int len1 = (p1.phase == PaddlePhase::Charging) ? p1.charge : p1.ext;
  uint32_t c1;
  if (p1.phase == PaddlePhase::Charging)      c1 = leds.rgb(30, 90, 30);
  else if (p1.phase == PaddlePhase::Strike)   c1 = leds.rgb(20, 230, 20);
  else                                        c1 = leds.rgb(18, 18, 18);
  for (int i = 0; i < len1; i++) leds.setLogical(i, c1);

  // шпингалет P2 (синий)
  int len2 = (p2.phase == PaddlePhase::Charging) ? p2.charge : p2.ext;
  uint32_t c2;
  if (p2.phase == PaddlePhase::Charging)      c2 = leds.rgb(30, 30, 90);
  else if (p2.phase == PaddlePhase::Strike)   c2 = leds.rgb(20, 20, 230);
  else                                        c2 = leds.rgb(18, 18, 18);
  for (int i = 0; i < len2; i++)
    leds.setLogical(LED_COUNT - 1 - i, c2);
}

void PongGame::drawOLED(Display& dpy) {
  char l1[22], l2[24];
  if (over) {
    snprintf(l1, sizeof l1, "P%d WINS THE MATCH", over);
    l2[0] = 0;
  } else {
    snprintf(l1, sizeof l1, "ROUND %d", round);
    snprintf(l2, sizeof l2, "E %d : %d", p1.charge, p2.charge);
  }
  dpy.gameFrame("PONG", score[0], score[1], l1, l2);
}

// ============================== GameManager ==============================

GameManager::GameManager() {
  _games[0] = &_tug;
  _games[1] = &_shooter;
  _games[2] = &_pong;
}

Game* GameManager::now() { return _games[_cur]; }

void GameManager::next() { _cur = (_cur + 1) % 3; }

void GameManager::resetCurrent() { now()->reset(); }

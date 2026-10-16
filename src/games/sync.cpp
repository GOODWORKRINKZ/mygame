#include "games.h"
#include <stdlib.h>
#include <math.h>

// ============================================================
//  SYNC — синхрон (кооператив на двоих)
// ============================================================
//  Первая игра, где двое не соперники: два огня летят с краёв
//  навстречу друг другу и встречаются в центре. Оба игрока должны
//  нажать ровно в этот момент — и попасть в общее окно допуска.
//  Каждый пройденный уровень окно сужается (260 мс → 45 мс), так
//  что скоро приходится ловить не глазами, а слухом: бузер ведёт
//  слайд, который заканчивается точно во время встречи.
//  Жизни, счёт и рекорд общие.

static const char* KEY_BEST = "sync";
static const float CENTER = (LED_COUNT - 1) / 2.0f;

void SyncGame::reset() {
  tol = SYNC_TOL_START;
  level = 1;
  lives = SYNC_LIVES;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  lastOk = false;
  over = false;
  overUntil = 0;
  event = "";
  arm(millis());
}

void SyncGame::arm(uint32_t now) {
  ph = Ph::Ready;
  phaseUntil = now + 900;
  startAt = 0;
  meetAt = 0;
  fired[0] = fired[1] = false;
  err[0] = err[1] = 0;
}

void SyncGame::resolve(Ctx& c) {
  bool ok = fired[0] && fired[1] &&
            abs(err[0]) <= (int)tol && abs(err[1]) <= (int)tol;
  lastOk = ok;
  ph = Ph::Judge;
  phaseUntil = c.now + 1200;

  if (ok) {
    score++;
    level++;
    tol = (uint16_t)(tol * SYNC_TOL_UP);
    if (tol < SYNC_TOL_MIN) tol = SYNC_TOL_MIN;
    c.out.sfx(Sfx::LevelUp);
    c.out.buzz(40);
    c.fx.burst(CENTER, rgb(0, 255, 200), 14, 24.0f);
    c.fx.flash(rgb(0, 200, 160), 220);
    event = "СИНХРОН!";
  } else {
    lives--;
    c.out.sfx(Sfx::Error);
    c.out.buzz(90);
    c.fx.flash(rgb(160, 0, 0), 260);
    if (!fired[0] || !fired[1]) event = "КТО-ТО НЕ НАЖАЛ";
    else if (abs(err[0] - err[1]) > (int)tol) event = "ВРАЗНОБОЙ";
    else event = "МИМО ЦЕНТРА";

    if (lives <= 0) {
      over = true;
      overUntil = c.now + 1200;
      newRecord = store.submit(KEY_BEST, score);
      best = store.best(KEY_BEST);
      c.out.sfx(Sfx::GameOver);
    }
  }
}

void SyncGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(0, 60, 60), rgb(0, 20, 20));
    char sub[22];
    snprintf(sub, sizeof sub, "УРОВЕНЬ %d", score);
    Ui::banner(c.dpy, "РАЗЛАД", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };

  switch (ph) {
    case Ph::Ready:
      if (c.now >= phaseUntil) {
        ph = Ph::Fly;
        startAt = c.now;
        meetAt = c.now + SYNC_TRAVEL_MS;
        // слайд заканчивается ровно в момент встречи — подсказка на слух
        c.out.slide(300, 1400, SYNC_TRAVEL_MS);
      }
      break;

    case Ph::Fly: {
      for (int p = 0; p < 2; p++) {
        if (fired[p] || !btn[p]->pressed) continue;
        fired[p] = true;
        err[p] = (int)((int32_t)c.now - (int32_t)meetAt);
        c.out.tone(p == 0 ? 900 : 1200, 25);
      }
      // решаем, когда оба нажали или когда терпеть уже нечего
      if ((fired[0] && fired[1]) || c.now > meetAt + tol + 200) resolve(c);
      break;
    }

    case Ph::Judge:
      if (c.now >= phaseUntil) arm(c.now);
      break;
  }

  render(c);
  oled(c);
}

void SyncGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  switch (ph) {
    case Ph::Ready: {
      // оба края дышат своим цветом: приготовились
      uint8_t b = (uint8_t)(30 + sin8((uint8_t)(c.now / 4)) / 3);
      L.set(0, colScale(COL_P1, b));
      L.set(LED_COUNT - 1, colScale(COL_P2, b));
      L.addAA(CENTER, rgb(20, 20, 20));
      break;
    }

    case Ph::Fly: {
      float t = (float)(c.now - startAt) / (float)SYNC_TRAVEL_MS;
      float p1 = t * CENTER;
      float p2 = (LED_COUNT - 1.0f) - t * CENTER;

      // окно допуска — тусклая зона вокруг центра
      float half = (tol / 1000.0f) * (CENTER / (SYNC_TRAVEL_MS / 1000.0f));
      if (half < 0.5f) half = 0.5f;
      for (int i = 0; i < LED_COUNT; i++) {
        if (fabsf((float)i - CENTER) <= half) L.set(i, rgb(12, 16, 16));
      }

      L.addAA(p1 - 1.0f, colScale(COL_P1, 40));
      L.addAA(p2 + 1.0f, colScale(COL_P2, 40));
      L.addAA(p1, fired[0] ? rgb(255, 255, 255) : COL_P1);
      L.addAA(p2, fired[1] ? rgb(255, 255, 255) : COL_P2);
      break;
    }

    case Ph::Judge: {
      Color col = lastOk ? rgb(0, 255, 180) : rgb(200, 0, 0);
      uint8_t b = ((c.now / 100) & 1) ? 160 : 60;
      if (lastOk) {
        for (int i = 0; i < LED_COUNT; i++) {
          float d = fabsf((float)i - CENTER);
          L.set(i, colScale(col, (uint8_t)(200 / (1.0f + d * 0.5f))));
        }
      } else {
        L.fillAll(colScale(col, b));
      }
      break;
    }
  }

  // жизни — точки на самом краю
  for (int k = 0; k < lives; k++) L.add(k, rgb(0, 30, 20));
}

void SyncGame::oled(Ctx& c) {
  char b1[10], b2[10], foot[26], right[8];
  const char* s1 = "--";
  const char* s2 = "--";

  for (int p = 0; p < 2; p++) {
    char* dst = (p == 0) ? b1 : b2;
    const char** sub = (p == 0) ? &s1 : &s2;
    if (!fired[p]) {
      snprintf(dst, 10, "--");
      *sub = (ph == Ph::Fly) ? "ЖДЕМ" : "ГОТОВ";
    } else {
      snprintf(dst, 10, "%d", abs(err[p]));
      *sub = (err[p] < 0) ? "РАНО" : "ПОЗДНО";
      if (abs(err[p]) <= (int)tol) *sub = "В ОКНО";
    }
  }

  snprintf(right, sizeof right, "У%d", level);
  snprintf(foot, sizeof foot, "ОКНО %dмс  ЖИЗНИ %d", tol, lives);

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.active = fired[1];
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.active = fired[0];

  const char* f = foot;
  if (ph == Ph::Judge) f = event;

  Ui::split(c.dpy, name(), right, l, r, f);
}

#include "games.h"

// ============================================================
//  SIMON — повтори последовательность (1 игрок)
// ============================================================
//  Лента показывает серию вспышек: левая половина (зелёная) или
//  правая (синяя). Каждая со своей нотой. Потом повторяешь ту же
//  серию кнопками. Ошибся или задумался дольше 4 секунд — конец.
//  С каждым уровнем показ становится быстрее.

static const char* KEY_BEST = "simon";
static const uint16_t TONE_LEFT  = 440;
static const uint16_t TONE_RIGHT = 659;

void SimonGame::reset() {
  len = 0;
  showIdx = 0;
  inputIdx = 0;
  showMs = 480;
  best = store.best(KEY_BEST);
  newRecord = false;
  flashSide = -1;
  flashUntil = 0;
  ph = Ph::Intro;
  phaseUntil = millis() + 700;
}

void SimonGame::nextLevel(Ctx& c) {
  if (len < SIMON_MAX_LEN) seq[len++] = (uint8_t)random(2);
  showMs = (uint16_t)(len < 20 ? 480 - len * 14 : 200);
  if (showMs < 190) showMs = 190;
  startShow(c);
}

void SimonGame::startShow(Ctx& c) {
  showIdx = 0;
  inputIdx = 0;
  ph = Ph::Gap;
  phaseUntil = c.now + 400;
  flashSide = -1;
}

void SimonGame::update(const Inputs& in, Ctx& c) {
  switch (ph) {
    case Ph::Intro:
      if (c.now >= phaseUntil) nextLevel(c);
      break;

    case Ph::Gap:
      if (c.now >= phaseUntil) {
        if (showIdx >= len) {
          // показ закончен — ход игрока
          ph = Ph::Input;
          inputIdx = 0;
          phaseUntil = c.now + 4000;
          flashSide = -1;
          c.out.sfx(Sfx::Beep);
        } else {
          ph = Ph::Show;
          flashSide = seq[showIdx];
          phaseUntil = c.now + (uint32_t)(showMs * 6 / 10);
          c.out.tone(flashSide == 0 ? TONE_LEFT : TONE_RIGHT,
                     (uint16_t)(showMs * 6 / 10));
        }
      }
      break;

    case Ph::Show:
      if (c.now >= phaseUntil) {
        showIdx++;
        flashSide = -1;
        ph = Ph::Gap;
        phaseUntil = c.now + (uint32_t)(showMs * 4 / 10);
      }
      break;

    case Ph::Input: {
      int pressed = -1;
      if (in.p1.pressed) pressed = 0;
      else if (in.p2.pressed) pressed = 1;

      if (pressed >= 0) {
        flashSide = pressed;
        flashUntil = c.now + 160;
        c.out.tone(pressed == 0 ? TONE_LEFT : TONE_RIGHT, 150);
        c.out.buzz(20);

        if (seq[inputIdx] == (uint8_t)pressed) {
          inputIdx++;
          phaseUntil = c.now + 4000;
          if (inputIdx >= len) {
            ph = Ph::Good;
            phaseUntil = c.now + 700;
            c.out.sfx(Sfx::LevelUp);
            c.fx.flash(rgb(0, 200, 60), 250);
          }
        } else {
          ph = Ph::Bad;
          phaseUntil = c.now + 1000;
          c.out.sfx(Sfx::Error);
          c.fx.flash(rgb(200, 0, 0), 400);
        }
      } else if (c.now >= phaseUntil) {
        // задумался слишком надолго
        ph = Ph::Bad;
        phaseUntil = c.now + 1000;
        c.out.sfx(Sfx::Error);
      }
      break;
    }

    case Ph::Good:
      if (c.now >= phaseUntil) nextLevel(c);
      break;

    case Ph::Bad:
      if (c.now >= phaseUntil) {
        newRecord = store.submit(KEY_BEST, len - 1 > 0 ? len - 1 : 0);
        best = store.best(KEY_BEST);
        ph = Ph::Over;
        phaseUntil = c.now + 1200;
        c.out.sfx(Sfx::GameOver);
      }
      break;

    case Ph::Over:
      if (c.now >= phaseUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
      if (c.now >= phaseUntil + 8000) { reset(); return; }
      break;
  }

  render(c);
  oled(c);
}

void SimonGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  if (ph == Ph::Over) {
    Bg::confetti(c.leds, c.now, rgb(120, 0, 0), rgb(40, 0, 0));
    return;
  }

  const int HALF = LED_COUNT / 2;

  // тусклая разметка половин, чтобы всегда было видно "поле"
  for (int i = 0; i < HALF; i++)          L.set(i, colScale(COL_P1, 18));
  for (int i = HALF; i < LED_COUNT; i++)  L.set(i, colScale(COL_P2, 18));

  int side = flashSide;
  if (side >= 0 && ph == Ph::Input && c.now >= flashUntil) side = -1;

  if (side == 0)      for (int i = 0; i < HALF; i++)         L.set(i, COL_P1);
  else if (side == 1) for (int i = HALF; i < LED_COUNT; i++) L.set(i, COL_P2);

  if (ph == Ph::Good) L.addAll(rgb(0, 60, 20));
  if (ph == Ph::Bad)  L.addAll(rgb(80, 0, 0));

  // индикатор прогресса ввода: сколько шагов уже повторил
  if (ph == Ph::Input) {
    for (int k = 0; k < inputIdx && k < LED_COUNT; k++)
      L.add(k, rgb(30, 30, 30));
  }
}

void SimonGame::oled(Ctx& c) {
  if (ph == Ph::Over) {
    char sub[20];
    snprintf(sub, sizeof sub, "УРОВЕНЬ %d", len - 1 > 0 ? len - 1 : 0);
    Ui::banner(c.dpy, "КОНЕЦ ИГРЫ", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  char big[10], l1[24], l2[24], right[8];
  snprintf(big, sizeof big, "%d", len);
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, "%d", len);

  const char* foot;
  switch (ph) {
    case Ph::Show:
    case Ph::Gap:   foot = "СМОТРИ..."; break;
    case Ph::Input: foot = "ТВОЙ ХОД"; break;
    case Ph::Good:  foot = "ВЕРНО!"; break;
    case Ph::Bad:   foot = "ОШИБКА!"; break;
    default:        foot = "ПРИГОТОВЬСЯ"; break;
  }

  if (ph == Ph::Input) snprintf(l1, sizeof l1, "ШАГ %d / %d", inputIdx + 1, len);
  else                 snprintf(l1, sizeof l1, "ДЛИНА %d", len);

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

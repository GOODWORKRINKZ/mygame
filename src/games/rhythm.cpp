#include "games.h"
#include <math.h>

// ============================================================
//  RHYTHM — ритм (1 игрок)
// ============================================================
//  Бузер отбивает такт, ноты выезжают с дальнего края и доезжают
//  до наковальни ровно за две доли. Задача — жать ЗЕЛЁНУЮ в тот
//  момент, когда нота стоит на наковальне. Попадание в самый
//  центр даёт больше очков и растит комбо, каждые восемь удачных
//  нот темп подрастает. Единственная игра, где бузер не сигналит,
//  а задаёт ритм: играть проще на слух, чем на глаз.

static const char* KEY_BEST = "rhythm";

void RhythmGame::reset() {
  for (auto& n : notes) n.active = false;
  beatMs = RHYTHM_BEAT_START;
  nextBeat = millis() + 900;
  speed = (float)(LED_COUNT - 1 - RHYTHM_HIT_POS) / (2.0f * beatMs / 1000.0f);
  lives = RHYTHM_LIVES;
  score = 0;
  combo = 0;
  bestCombo = 0;
  hits = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  judgeUntil = 0;
  judge = 0;
}

void RhythmGame::spawn() {
  for (auto& n : notes) {
    if (n.active) continue;
    n.active = true;
    n.pos = (float)(LED_COUNT - 1);
    return;
  }
}

void RhythmGame::miss(Ctx& c) {
  combo = 0;
  lives--;
  judge = 3;
  judgeUntil = c.now + 300;
  c.out.sfx(Sfx::Error);
  c.out.buzz(50);
  c.fx.flash(rgb(150, 0, 0), 180);

  if (lives <= 0) {
    over = true;
    overUntil = c.now + 1200;
    newRecord = store.submit(KEY_BEST, score);
    best = store.best(KEY_BEST);
    c.out.sfx(Sfx::GameOver);
  }
}

void RhythmGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(90, 0, 40), rgb(30, 0, 15));
    char sub[22];
    snprintf(sub, sizeof sub, "ОЧКИ %d  КОМБО %d", score, bestCombo);
    Ui::banner(c.dpy, "СБИЛСЯ", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- метроном: тик каждую долю, ноты не на каждой ----
  if (c.now >= nextBeat) {
    nextBeat = c.now + beatMs;
    c.out.tone(1500, 10);
    if (random(100) < 78) spawn();
  }

  // ---- движение нот ----
  for (auto& n : notes) {
    if (!n.active) continue;
    n.pos -= speed * c.dt;
    if (n.pos < RHYTHM_HIT_POS - 1.7f) {
      n.active = false;
      miss(c);
      if (over) return;
    }
  }

  // ---- удар ----
  if (in.p1.pressed) {
    int bestIdx = -1;
    float bestErr = 999.0f;
    for (int i = 0; i < RHYTHM_MAX_NOTES; i++) {
      if (!notes[i].active) continue;
      float e = fabsf(notes[i].pos - (float)RHYTHM_HIT_POS);
      if (e < bestErr) { bestErr = e; bestIdx = i; }
    }

    if (bestIdx >= 0 && bestErr <= 1.7f) {
      notes[bestIdx].active = false;
      combo++;
      hits++;
      if (combo > bestCombo) bestCombo = combo;

      if (bestErr <= 0.7f) {
        judge = 1;
        score += 10 + combo;
        c.out.tone(1760, 60);
        c.fx.burst((float)RHYTHM_HIT_POS, rgb(255, 255, 255), 8, 20.0f);
      } else {
        judge = 2;
        score += 5;
        c.out.tone(1180, 50);
        c.fx.spark((float)RHYTHM_HIT_POS, 10.0f, rgb(255, 0, 120), 0.3f);
      }
      judgeUntil = c.now + 260;
      c.out.buzz(15);

      // каждые 8 нот — темп выше, ноты едут быстрее
      if (hits % 8 == 0 && beatMs > RHYTHM_BEAT_MIN) {
        beatMs = (uint16_t)(beatMs * RHYTHM_BEAT_UP);
        if (beatMs < RHYTHM_BEAT_MIN) beatMs = RHYTHM_BEAT_MIN;
        speed = (float)(LED_COUNT - 1 - RHYTHM_HIT_POS) / (2.0f * beatMs / 1000.0f);
        c.out.sfx(Sfx::LevelUp);
      }
    } else {
      // ударил в пустоту
      miss(c);
      if (over) return;
    }
  }

  render(c);
  oled(c);
}

void RhythmGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- наковальня: окно попадания + пульс на долю ----
  uint32_t sinceBeat = c.now + beatMs - nextBeat;
  uint8_t pulse = (sinceBeat < 120) ? (uint8_t)(200 - sinceBeat) : 60;
  L.set(RHYTHM_HIT_POS - 1, rgb(20, 20, 24));
  L.set(RHYTHM_HIT_POS + 1, rgb(20, 20, 24));
  L.set(RHYTHM_HIT_POS, colScale(rgb(255, 255, 255), pulse));

  // ---- ноты: чем ближе к наковальне, тем горячее ----
  for (auto& n : notes) {
    if (!n.active) continue;
    float d = n.pos - (float)RHYTHM_HIT_POS;
    Color col = (d < 2.0f) ? rgb(255, 60, 0) : rgb(255, 0, 140);
    L.addAA(n.pos, col);
    L.addAA(n.pos + 0.9f, colScale(col, 40));
  }

  // ---- оценка последнего удара ----
  if (c.now < judgeUntil) {
    Color j = (judge == 1) ? rgb(0, 255, 120)
                           : (judge == 2 ? rgb(255, 200, 0) : rgb(255, 0, 0));
    uint8_t b = (uint8_t)(((judgeUntil - c.now) * 120) / 300);
    L.addAll(colScale(j, b));
  }

  // ---- жизни на дальнем краю ----
  for (int k = 0; k < lives; k++) L.add(LED_COUNT - 1 - k, rgb(50, 0, 0));
}

void RhythmGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[26];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "КОМБО %d  ТЕМП %d", combo, (int)(60000UL / beatMs));
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, "x%d", combo);

  const char* word = "ЖМИ В ДОЛЮ";
  if (c.now < judgeUntil) {
    word = (judge == 1) ? "ТОЧНО!" : (judge == 2 ? "ХОРОШО" : "МИМО");
  }
  char hearts[8] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(foot, sizeof foot, "%s  %s", word, hearts);

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

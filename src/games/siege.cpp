#include "games.h"
#include <math.h>

// ============================================================
//  SIEGE — осада (кооператив на двоих)
// ============================================================
//  ЗАЩИТА, вывернутая наизнанку: база стоит в центре ленты, а
//  враги лезут сразу с двух краёв. Зелёный держит левый фланг,
//  синий — правый, стрелять можно только в свою сторону. Жизни,
//  счёт, волны и рекорд общие — проигрывают тоже вместе, поэтому
//  выгоднее орать соседу "у меня броневик", чем молча стрелять.

static const char* KEY_BEST = "siege";
static const int   BASE_C = LED_COUNT / 2;        // центр базы
static const float CANNON_L = (float)(BASE_C - 2);
static const float CANNON_R = (float)(BASE_C + 2);

void SiegeGame::reset() {
  for (auto& f : foes) f.active = false;
  for (auto& s : shots) s.active = false;
  lives = SIEGE_LIVES;
  score = 0;
  wave = 1;
  kills = 0;
  spawnIn = 1.2f;
  cd[0] = cd[1] = 0;
  hurtUntil = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
}

void SiegeGame::spawnFoe() {
  int slot = -1;
  for (int i = 0; i < SIEGE_MAX_ENEMY; i++)
    if (!foes[i].active) { slot = i; break; }
  if (slot < 0) return;

  int roll = (int)random(100);
  uint8_t kind = 0;
  if (wave >= 4 && roll >= 95)     kind = 2;
  else if (wave >= 2 && roll < 28) kind = 1;

  Foe& f = foes[slot];
  f.active = true;
  f.kind = kind;
  f.side = (random(2) == 0) ? -1 : 1;
  f.pos = (f.side < 0) ? 0.0f : (float)(LED_COUNT - 1);
  f.flashUntil = 0;

  float base = 1.1f + wave * 0.11f;
  if (base > 4.5f) base = 4.5f;
  switch (kind) {
    case 1:  f.hp = 3;                  base *= 0.7f;  break;
    case 2:  f.hp = (int8_t)(7 + wave); base *= 0.45f; break;
    default: f.hp = 1;                                 break;
  }
  f.vel = -f.side * base;      // всегда в сторону центра
}

void SiegeGame::shoot(int p, Ctx& c) {
  if (c.now < cd[p]) return;
  for (auto& s : shots) {
    if (s.active) continue;
    s.active = true;
    s.dir = (p == 0) ? -1 : 1;
    s.pos = (p == 0) ? CANNON_L : CANNON_R;
    cd[p] = c.now + SIEGE_COOLDOWN_MS;
    c.out.sfx(Sfx::Shoot);
    c.fx.jet(s.pos, s.dir, p == 0 ? COL_P1 : COL_P2, 2, 12.0f);
    return;
  }
}

void SiegeGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(120, 30, 0), rgb(40, 10, 0));
    char sub[22];
    snprintf(sub, sizeof sub, "ВОЛНА %d  ОЧКИ %d", wave, score);
    Ui::banner(c.dpy, "БАЗА ПАЛА", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  if (in.p1.pressed) shoot(0, c);
  if (in.p2.pressed) shoot(1, c);

  // ---- спавн ----
  spawnIn -= c.dt;
  if (spawnIn <= 0) {
    spawnFoe();
    spawnIn = ((float)random(70, 150) / 100.0f) * (2.2f / (1.0f + wave * 0.14f));
    if (spawnIn < 0.3f) spawnIn = 0.3f;
  }

  // ---- снаряды ----
  for (auto& s : shots) {
    if (!s.active) continue;
    s.pos += s.dir * 22.0f * c.dt;
    if (s.pos < -1.0f || s.pos > LED_COUNT) { s.active = false; continue; }

    for (auto& f : foes) {
      if (!f.active || f.side != s.dir) continue;
      if (fabsf(f.pos - s.pos) > 0.8f) continue;

      s.active = false;
      f.hp--;
      f.flashUntil = c.now + 120;
      c.out.sfx(Sfx::Hit);

      if (f.hp <= 0) {
        f.active = false;
        kills++;
        score += (f.kind == 2) ? 100 : (f.kind == 1 ? 25 : 10);
        c.fx.burst(f.pos, rgb(255, 140, 0), 8, 18.0f);
        if (kills % SIEGE_KILLS_PER_WAVE == 0) {
          wave++;
          c.out.sfx(Sfx::LevelUp);
          c.fx.flash(rgb(60, 60, 120), 220);
        }
      }
      break;
    }
  }

  // ---- враги ----
  for (auto& f : foes) {
    if (!f.active) continue;
    f.pos += f.vel * c.dt;

    bool reached = (f.side < 0) ? (f.pos >= BASE_C - 1) : (f.pos <= BASE_C + 1);
    if (reached) {
      f.active = false;
      lives--;
      hurtUntil = c.now + 400;
      c.out.sfx(Sfx::Explode);
      c.out.buzzPattern(2, 90, 60);
      c.fx.shock((float)BASE_C, rgb(255, 0, 0), 60.0f, 400);

      if (lives <= 0) {
        over = true;
        overUntil = c.now + 1200;
        newRecord = store.submit(KEY_BEST, score);
        best = store.best(KEY_BEST);
        c.out.sfx(Sfx::GameOver);
        return;
      }
    }
  }

  render(c);
  oled(c);
}

void SiegeGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- база в центре: яркость = остаток жизней ----
  uint8_t bb = (uint8_t)(60 + (lives * 190) / SIEGE_LIVES);
  if (c.now < hurtUntil) bb = ((c.now / 60) & 1) ? 255 : 80;
  Color baseCol = (c.now < hurtUntil) ? rgb(255, 40, 0) : rgb(180, 180, 255);
  for (int i = BASE_C - 1; i <= BASE_C + 1; i++) L.set(i, colScale(baseCol, bb));

  // ---- пушки флангов ----
  L.set((int)CANNON_L, colScale(COL_P1, c.now < cd[0] ? 40 : 150));
  L.set((int)CANNON_R, colScale(COL_P2, c.now < cd[1] ? 40 : 150));

  // ---- враги ----
  for (auto& f : foes) {
    if (!f.active) continue;
    Color col;
    switch (f.kind) {
      case 1:  col = rgb(180, 0, 255); break;
      case 2:  col = rgb(255, 120, 0); break;
      default: col = rgb(255, 20, 20); break;
    }
    if (c.now < f.flashUntil) col = rgb(255, 255, 255);
    L.addAA(f.pos, col);
    if (f.kind == 2) {                      // босс толще
      L.addAA(f.pos - f.side * 1.0f, colScale(col, 120));
    }
  }

  // ---- снаряды ----
  for (auto& s : shots) {
    if (!s.active) continue;
    L.addAA(s.pos, rgb(255, 240, 160));
  }
}

void SiegeGame::oled(Ctx& c) {
  int nLeft = 0, nRight = 0;
  for (auto& f : foes) {
    if (!f.active) continue;
    if (f.side < 0) nLeft++; else nRight++;
  }

  char b1[6], b2[6], right[8], foot[26];
  snprintf(b1, sizeof b1, "%d", nLeft);
  snprintf(b2, sizeof b2, "%d", nRight);
  snprintf(right, sizeof right, "В%d", wave);
  snprintf(foot, sizeof foot, "ОЧКИ %d  БАЗА %d", score, lives);

  Panel l, r;
  // Левая панель — синий: он держит правый фланг ленты.
  l.name = "СИНИЙ";   l.big = b2; l.sub = "ПР ФЛАНГ";  l.active = (c.now < cd[1]);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = "ЛЕВ ФЛАНГ";  r.active = (c.now < cd[0]);

  Ui::split(c.dpy, name(), right, l, r, foot);
}

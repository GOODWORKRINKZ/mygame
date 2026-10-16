#include "games.h"
#include <math.h>

// ============================================================
//  SIEGE — осада (кооператив на двоих), на реальной геометрии
// ============================================================
//  База — сердцевина матрицы (2x2). Враги появляются у кнопок и
//  ползут по рукавам к воротам: зелёный держит свой рукав, синий —
//  свой, пушки стоят у ворот и стреляют наружу, по своему рукаву.
//  Враг, дошедший до ворот, выходит на кольцо и идёт по нему вокруг
//  базы; если за полный круг его не сняли — он бьёт базу. На кольце
//  врага можно добить в упор, когда он проходит мимо твоих ворот:
//  выстрел в этот момент не летит по рукаву, а бьёт его на месте.
//  То есть враг, прорвавшийся у зелёного, через полкруга оказывается
//  у ворот синего — и это общая проблема, а не чья-то. Жизни, счёт,
//  волны и рекорд общие.

static const char* KEY_BEST = "siege";
static const float RING_LEN   = (float)RING.len;
static const float GATE_REACH = 0.9f;     // "в упор": враг не дальше этого от ворот

static inline const Path& armOf(int8_t side) { return (side < 0) ? ARM_P1 : ARM_P2; }
static inline int gateOf(int8_t side)         { return (side < 0) ? RING_GATE_P1 : RING_GATE_P2; }

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
  f.pos = 0.0f;                       // у кнопки, ползёт к воротам (индекс растёт)
  f.onRing = false;
  f.ringPos = 0;
  f.travel = 0;
  f.dir = random(2) ? 1 : -1;         // в какую сторону пойдёт по кольцу
  f.flashUntil = 0;

  float base = 1.1f + wave * 0.11f;
  if (base > 4.5f) base = 4.5f;
  switch (kind) {
    case 1:  f.hp = 3;                  base *= 0.7f;  break;
    case 2:  f.hp = (int8_t)(7 + wave); base *= 0.45f; break;
    default: f.hp = 1;                                 break;
  }
  f.vel = base;
}

void SiegeGame::killFoe(Foe& f, Ctx& c) {
  f.active = false;
  kills++;
  score += (f.kind == 2) ? 100 : (f.kind == 1 ? 25 : 10);
  if (f.onRing) c.fx.burst(f.ringPos, rgb(255, 140, 0), 8, 16.0f, &RING);
  else          c.fx.burst(f.pos, rgb(255, 140, 0), 8, 16.0f, &armOf(f.side));
  if (kills % SIEGE_KILLS_PER_WAVE == 0) {
    wave++;
    c.out.sfx(Sfx::LevelUp);
    c.fx.flash(rgb(60, 60, 120), 220);
  }
}

void SiegeGame::shoot(int p, Ctx& c) {
  if (c.now < cd[p]) return;
  int8_t side = (p == 0) ? -1 : 1;
  cd[p] = c.now + SIEGE_COOLDOWN_MS;

  // ---- враг в упор у моих ворот на кольце? бьём его на месте ----
  int gate = gateOf(side);
  Foe* near = nullptr;
  float bestD = 99.0f;
  for (auto& f : foes) {
    if (!f.active || !f.onRing) continue;
    float d = fabsf(RING.ahead((float)gate, f.ringPos));
    if (d > RING_LEN / 2) d = RING_LEN - d;
    if (d <= GATE_REACH && d < bestD) { bestD = d; near = &f; }
  }
  if (near) {
    near->hp -= 2;                       // в упор бьём вдвое больнее
    near->flashUntil = c.now + 120;
    c.out.sfx(Sfx::Hit);
    c.out.buzz(20);
    c.fx.spark((float)gate, (float)near->dir * 8.0f, p == 0 ? COL_P1 : COL_P2, 0.25f, &RING);
    if (near->hp <= 0) killFoe(*near, c);
    return;
  }

  // ---- иначе обычный выстрел наружу по своему рукаву ----
  for (auto& s : shots) {
    if (s.active) continue;
    s.active = true;
    s.side = side;
    s.pos = (float)(armOf(side).len - 1);      // от ворот к кнопке (индекс убывает)
    c.out.sfx(Sfx::Shoot);
    c.fx.jet(s.pos, -1, p == 0 ? COL_P1 : COL_P2, 2, 10.0f, &armOf(side));
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

  // ---- снаряды: летят по рукаву наружу ----
  for (auto& s : shots) {
    if (!s.active) continue;
    s.pos -= 22.0f * c.dt;
    if (s.pos < -1.0f) { s.active = false; continue; }

    for (auto& f : foes) {
      if (!f.active || f.onRing || f.side != s.side) continue;
      float halfW = (f.kind == 2) ? 1.2f : 0.8f;
      if (fabsf(f.pos - s.pos) > halfW) continue;

      s.active = false;
      f.hp--;
      f.flashUntil = c.now + 120;
      c.out.sfx(Sfx::Hit);
      if (f.hp <= 0) killFoe(f, c);
      break;
    }
  }

  // ---- враги ----
  for (auto& f : foes) {
    if (!f.active) continue;

    if (!f.onRing) {
      f.pos += f.vel * c.dt;
      if (f.pos >= (float)(armOf(f.side).len - 1)) {
        // дошёл до ворот — выходит на кольцо
        f.onRing = true;
        f.ringPos = (float)gateOf(f.side);
        f.travel = 0;
        c.out.tone(260, 60);
        c.out.buzz(20);
      }
      continue;
    }

    float step = f.vel * SIEGE_RING_SPEED_MUL * c.dt;
    f.ringPos = RING.wrap(f.ringPos + f.dir * step);
    f.travel += step;
    if (f.travel >= RING_LEN) {
      // полный круг вокруг базы — удар
      f.active = false;
      lives--;
      hurtUntil = c.now + 400;
      c.out.sfx(Sfx::Explode);
      c.out.buzzPattern(2, 90, 60);
      c.fx.burst(f.ringPos, rgb(255, 0, 0), 10, 18.0f, &RING);
      c.fx.flash(rgb(120, 0, 0), 200);

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

  // ---- рукава: тусклая земля своим цветом, пушка у ворот ----
  for (int p = 0; p < 2; p++) {
    const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    int8_t side = (p == 0) ? -1 : 1;
    for (int i = 0; i < arm.len; i++) L.set(arm, i, colScale(col, 10));
    L.set(arm, arm.len - 1, colScale(col, c.now < cd[p] ? 50 : 170));

    // враг проходит мимо моих ворот — пушка и ворота вспыхивают белым: "бей сейчас"
    bool reach = false;
    for (auto& f : foes) {
      if (!f.active || !f.onRing) continue;
      float d = RING.ahead((float)gateOf(side), f.ringPos);
      if (d > RING_LEN / 2) d = RING_LEN - d;
      if (d <= GATE_REACH) { reach = true; break; }
    }
    if (reach) {
      uint8_t b = ((c.now / 70) & 1) ? 255 : 90;
      L.add(arm, arm.len - 1, colScale(rgb(255, 255, 255), b));
      L.add(RING, gateOf(side), colScale(rgb(255, 255, 255), b / 2));
    }
  }

  // ---- кольцо: стена вокруг базы ----
  for (int i = 0; i < RING.len; i++) L.add(RING, i, rgb(6, 6, 10));

  // ---- база: сердцевина, яркость = жизни ----
  uint8_t bb = (uint8_t)(50 + (lives * 200) / SIEGE_LIVES);
  Color baseCol = rgb(180, 180, 255);
  if (c.now < hurtUntil) { bb = ((c.now / 60) & 1) ? 255 : 80; baseCol = rgb(255, 40, 0); }
  L.fill(CORE, colScale(baseCol, bb));

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
    if (f.onRing) {
      // на кольце враг "горячеет" к концу круга — видно, что скоро ударит
      float k = f.travel / RING_LEN;
      Color hot = colLerp(col, rgb(255, 255, 255), (uint8_t)(k * 120));
      L.addAA(RING, f.ringPos, hot);
      L.addAA(RING, f.ringPos - f.dir * 0.8f, colScale(hot, 50));
      if (f.kind == 2) L.addAA(RING, f.ringPos - f.dir * 1.0f, colScale(hot, 140));
    } else {
      const Path& arm = armOf(f.side);
      L.addAA(arm, f.pos, col);
      if (f.kind == 2) L.addAA(arm, f.pos - 1.0f, colScale(col, 140));
    }
  }

  // ---- снаряды ----
  for (auto& s : shots) {
    if (!s.active) continue;
    L.addAA(armOf(s.side), s.pos, rgb(255, 240, 160));
  }
}

void SiegeGame::oled(Ctx& c) {
  int nLeft = 0, nRight = 0, nRing = 0;
  for (auto& f : foes) {
    if (!f.active) continue;
    if (f.onRing) nRing++;
    else if (f.side < 0) nLeft++;
    else nRight++;
  }

  char b1[6], b2[6], right[8], foot[26];
  snprintf(b1, sizeof b1, "%d", nLeft);
  snprintf(b2, sizeof b2, "%d", nRight);
  snprintf(right, sizeof right, "В%d", wave);
  if (nRing) snprintf(foot, sizeof foot, "НА КОЛЬЦЕ %d  БАЗА %d", nRing, lives);
  else       snprintf(foot, sizeof foot, "ОЧКИ %d  БАЗА %d", score, lives);

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = "МОЙ РУКАВ"; l.active = (c.now < cd[1]);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = "МОЙ РУКАВ"; r.active = (c.now < cd[0]);

  Ui::split(c.dpy, name(), right, l, r, foot);
}

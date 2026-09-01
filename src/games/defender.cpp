#include "games.h"
#include <math.h>

// ============================================================
//  DEFENDER — оборона базы (1 игрок)
// ============================================================
//  База слева (пиксели 0..DEFENDER_BASE-1), пушка сразу за ней.
//  Справа ползут враги: обычные (красные), бронированные (фиолетовые,
//  3 HP, медленнее) и боссы (оранжевые, толстые). ЗЕЛЁНАЯ — выстрел,
//  СИНЯЯ — импульс: всем врагам 2 урона и отбрасывание. Импульсов
//  мало, но за каждую волну дают ещё один.

static const char* KEY_BEST = "defend";
static const float CANNON = (float)DEFENDER_BASE;
static const int   KILLS_PER_WAVE = 8;

void DefenderGame::reset() {
  for (int i = 0; i < DEFENDER_MAX_ENEMY; i++) enemies[i].active = false;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  lives = DEFENDER_LIVES;
  score = 0;
  wave = 1;
  killsInWave = 0;
  nukes = DEFENDER_NUKES;
  spawnIn = 1.0f;
  cd = 0;
  nukeFlashUntil = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
}

void DefenderGame::spawnEnemy() {
  int slot = -1;
  for (int i = 0; i < DEFENDER_MAX_ENEMY; i++)
    if (!enemies[i].active) { slot = i; break; }
  if (slot < 0) return;

  int roll = (int)random(100);
  uint8_t kind = 0;
  if (wave >= 5 && roll >= 96)      kind = 2;
  else if (wave >= 3 && roll < 25)  kind = 1;

  Enemy& e = enemies[slot];
  e.active = true;
  e.kind = kind;
  e.pos = LED_COUNT - 1.0f;
  e.flashUntil = 0;

  float base = 1.2f + wave * 0.12f;
  if (base > 5.0f) base = 5.0f;
  switch (kind) {
    case 1:  e.hp = 3;                    e.vel = -base * 0.7f; break;
    case 2:  e.hp = (int8_t)(8 + wave);   e.vel = -base * 0.45f; break;
    default: e.hp = 1;                    e.vel = -base; break;
  }
}

void DefenderGame::shoot(Ctx& c) {
  if (c.now < cd) return;
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) continue;
    bullets[i].active = true;
    bullets[i].pos = CANNON + 0.5f;
    cd = c.now + 170;
    c.out.sfx(Sfx::Shoot);
    c.fx.jet(CANNON, 1, rgb(255, 220, 80), 2, 12.0f);
    return;
  }
}

void DefenderGame::nuke(Ctx& c) {
  if (nukes <= 0) { c.out.sfx(Sfx::Error); return; }
  nukes--;
  nukeFlashUntil = c.now + 300;
  c.out.sfx(Sfx::Explode);
  c.out.buzzPattern(2, 100, 60);
  c.fx.shock(CANNON, rgb(120, 200, 255), 70.0f, 500);

  for (int i = 0; i < DEFENDER_MAX_ENEMY; i++) {
    Enemy& e = enemies[i];
    if (!e.active) continue;
    e.hp -= 2;
    e.pos += 3.0f;
    if (e.pos > LED_COUNT - 1.0f) e.pos = LED_COUNT - 1.0f;
    e.flashUntil = c.now + 150;
    if (e.hp <= 0) {
      e.active = false;
      score += (e.kind == 2) ? 100 : (e.kind == 1 ? 25 : 10);
      killsInWave++;
      c.fx.burst(e.pos, rgb(255, 120, 0), 6, 14.0f);
    }
  }
}

void DefenderGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(120, 0, 0), rgb(50, 0, 0));
    char sub[20];
    snprintf(sub, sizeof sub, "SCORE %d", score);
    Ui::banner(c.dpy, "GAME OVER", sub,
               newRecord ? "NEW RECORD!" : "press to retry");
    return;
  }

  if (in.p1.pressed) shoot(c);
  if (in.p2.pressed) nuke(c);

  // ---- спавн ----
  spawnIn -= c.dt;
  if (spawnIn <= 0) {
    spawnEnemy();
    spawnIn = ((float)random(60, 140) / 100.0f) * (2.0f / (1.0f + wave * 0.12f));
    if (spawnIn < 0.25f) spawnIn = 0.25f;
  }

  // ---- пули ----
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    bullets[i].pos += 30.0f * c.dt;
    if (bullets[i].pos > LED_COUNT) bullets[i].active = false;
  }

  // ---- враги ----
  for (int i = 0; i < DEFENDER_MAX_ENEMY; i++) {
    Enemy& e = enemies[i];
    if (!e.active) continue;
    e.pos += e.vel * c.dt;

    // попадания
    for (int b = 0; b < MAX_BULLETS; b++) {
      if (!bullets[b].active) continue;
      float halfW = (e.kind == 2) ? 1.2f : 0.7f;
      if (fabsf(bullets[b].pos - e.pos) > halfW) continue;

      bullets[b].active = false;
      e.hp--;
      e.flashUntil = c.now + 90;
      c.out.tone(1400, 20);

      if (e.hp <= 0) {
        e.active = false;
        score += (e.kind == 2) ? 100 : (e.kind == 1 ? 25 : 10);
        killsInWave++;
        c.fx.burst(e.pos, e.kind == 2 ? rgb(255, 160, 0) : rgb(255, 60, 0),
                   e.kind == 2 ? 14 : 6, e.kind == 2 ? 24.0f : 14.0f);
        c.out.sfx(e.kind == 2 ? Sfx::Explode : Sfx::Hit);
      }
      break;
    }
    if (!e.active) continue;

    // добрался до базы
    if (e.pos <= CANNON - 0.5f) {
      e.active = false;
      lives--;
      c.fx.flash(rgb(200, 0, 0), 350);
      c.out.sfx(Sfx::Explode);
      c.out.buzzPattern(2, 120, 80);
      if (lives <= 0) {
        over = true;
        overUntil = c.now + 1300;
        newRecord = store.submit(KEY_BEST, score);
        best = store.best(KEY_BEST);
        c.out.sfx(Sfx::GameOver);
      }
    }
  }

  // ---- волна пройдена ----
  if (killsInWave >= KILLS_PER_WAVE) {
    killsInWave = 0;
    wave++;
    if (nukes < DEFENDER_NUKES) nukes++;
    c.out.sfx(Sfx::LevelUp);
    c.fx.flash(rgb(0, 120, 180), 300);
  }

  render(c);
  oled(c);
}

void DefenderGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // база: яркость = сколько жизней осталось
  uint8_t baseB = (uint8_t)(60 + (lives * 60));
  for (int i = 0; i < DEFENDER_BASE; i++)
    L.set(i, colScale(rgb(0, 160, 255), baseB));

  // пушка
  bool ready = (c.now >= cd);
  L.set(DEFENDER_BASE, ready ? rgb(255, 255, 255) : rgb(70, 70, 70));

  // враги
  for (int i = 0; i < DEFENDER_MAX_ENEMY; i++) {
    Enemy& e = enemies[i];
    if (!e.active) continue;
    Color col;
    switch (e.kind) {
      case 1:  col = rgb(190, 0, 255); break;
      case 2:  col = rgb(255, 130, 0); break;
      default: col = rgb(255, 30, 0); break;
    }
    if (c.now < e.flashUntil) col = rgb(255, 255, 255);
    L.addAA(e.pos, col);
    if (e.kind == 2) {                       // босс занимает три пикселя
      L.addAA(e.pos + 1.0f, colScale(col, 160));
      L.addAA(e.pos - 1.0f, colScale(col, 160));
    }
  }

  // пули
  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].active) L.addAA(bullets[i].pos, rgb(255, 230, 90));

  if (c.now < nukeFlashUntil)
    L.addAll(colScale(rgb(120, 200, 255), (uint8_t)(((nukeFlashUntil - c.now) * 255) / 300)));
}

void DefenderGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[10], foot[24];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "WAVE %d  NUKE %d", wave, nukes);
  snprintf(l2, sizeof l2, "BEST %d", best);
  snprintf(right, sizeof right, "W%d", wave);

  char hearts[8] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(foot, sizeof foot, "BASE %s   %d/%d", hearts, killsInWave, KILLS_PER_WAVE);

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

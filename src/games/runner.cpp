#include "games.h"
#include <math.h>

// ============================================================
//  RUNNER — бегун с препятствиями (1 игрок)
// ============================================================
//  Игрок стоит на позиции RUNNER_POS, навстречу летят препятствия:
//  КРАСНОЕ — надо прыгнуть (ЗЕЛЁНАЯ кнопка),
//  СИНЕЕ   — надо подкатиться (СИНЯЯ кнопка).
//  Действие живёт RUNNER_ACTION_MS — то есть важно не просто нажать,
//  а нажать вовремя. Скорость растёт вместе со счётом.

static const char* KEY_BEST = "runner";
static const float PX = (float)RUNNER_POS;

void RunnerGame::reset() {
  for (int i = 0; i < MAX_OBS; i++) obs[i].active = false;
  speed = 9.0f;
  spawnIn = 1.2f;
  lives = RUNNER_LIVES;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  action = 0;
  actionUntil = 0;
  hurtUntil = 0;
  over = false;
  overUntil = 0;
}

void RunnerGame::spawn() {
  for (int i = 0; i < MAX_OBS; i++) {
    if (obs[i].active) continue;
    obs[i].active = true;
    obs[i].judged = false;
    obs[i].pos = LED_COUNT - 1.0f;
    obs[i].kind = (uint8_t)random(2);
    return;
  }
}

void RunnerGame::update(const Inputs& in, Ctx& c) {
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

  // ---- ввод ----
  if (in.p1.pressed) {
    action = 1;
    actionUntil = c.now + RUNNER_ACTION_MS;
    c.out.tone(1200, 30);
  } else if (in.p2.pressed) {
    action = 2;
    actionUntil = c.now + RUNNER_ACTION_MS;
    c.out.tone(700, 30);
  }
  if (action && c.now >= actionUntil) action = 0;

  // ---- спавн ----
  spawnIn -= c.dt;
  if (spawnIn <= 0) {
    spawn();
    // расстояние между препятствиями держим примерно постоянным
    spawnIn = ((float)random(75, 155) / 100.0f) * (9.0f / speed);
  }

  // ---- движение и проверка ----
  for (int i = 0; i < MAX_OBS; i++) {
    Obstacle& o = obs[i];
    if (!o.active) continue;
    o.pos -= speed * c.dt;

    if (!o.judged && o.pos <= PX + 0.4f) {
      o.judged = true;
      uint8_t need = (uint8_t)(o.kind + 1);       // 1 прыжок, 2 подкат
      if (action == need) {
        score++;
        speed = 9.0f + score * 0.25f;
        if (speed > 26.0f) speed = 26.0f;
        c.out.sfx(Sfx::Pickup);
        c.fx.spark(PX, -14.0f, o.kind == 0 ? rgb(255, 80, 0) : rgb(0, 120, 255), 0.3f);
      } else {
        lives--;
        hurtUntil = c.now + 400;
        c.out.sfx(Sfx::Error);
        c.fx.flash(rgb(180, 0, 0), 250);
        c.out.buzzPattern(2, 70, 60);
        if (lives <= 0) {
          over = true;
          overUntil = c.now + 1200;
          newRecord = store.submit(KEY_BEST, score);
          best = store.best(KEY_BEST);
          c.out.sfx(Sfx::GameOver);
        }
      }
    }
    if (o.pos < -1.0f) o.active = false;
  }

  render(c);
  oled(c);
}

void RunnerGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // "земля": бегущая пунктирная дорожка — по ней видно скорость
  float sc = fmodf((float)c.now * 0.001f * speed, 4.0f);
  for (float x = -sc; x < LED_COUNT; x += 4.0f) L.addAA(x, rgb(12, 12, 16));

  // препятствия
  for (int i = 0; i < MAX_OBS; i++) {
    if (!obs[i].active) continue;
    Color col = (obs[i].kind == 0) ? rgb(255, 40, 0) : rgb(0, 90, 255);
    L.addAA(obs[i].pos, col);
    L.addAA(obs[i].pos + 0.8f, colScale(col, 60));
  }

  // игрок
  Color me;
  if (c.now < hurtUntil)   me = ((c.now / 60) & 1) ? rgb(255, 0, 0) : rgb(60, 0, 0);
  else if (action == 1)    me = rgb(255, 230, 40);     // прыжок
  else if (action == 2)    me = rgb(0, 230, 230);      // подкат
  else                     me = rgb(220, 220, 220);
  L.set(RUNNER_POS, me);
  if (action) L.add(RUNNER_POS, colScale(me, 120));

  // жизни — тусклые точки за спиной игрока
  for (int k = 0; k < lives; k++) L.add(k, rgb(30, 0, 0));
}

void RunnerGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[24];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "SPEED %d", (int)speed);
  snprintf(l2, sizeof l2, "BEST %d", best);
  snprintf(right, sizeof right, "x%d", lives);

  const char* act = (action == 1) ? "JUMP" : (action == 2) ? "SLIDE" : "";
  if (act[0]) snprintf(foot, sizeof foot, "%s", act);
  else        snprintf(foot, sizeof foot, "%s", hint());

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

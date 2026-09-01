#include "games.h"
#include <math.h>

// ============================================================
//  SNIPER — меткость (1 игрок)
// ============================================================
//  Курсор мечется по ленте, где-то светится зона-мишень.
//  Жми ЗЕЛЁНУЮ ровно в тот момент, когда курсор внутри зоны.
//  Попал — очки (в центр больше), зона сужается, курсор ускоряется.
//  Три промаха — конец. Рекорд сохраняется во флеше.

static const char* KEY_BEST = "sniper";

void SniperGame::reset() {
  cursor = 0;
  speed = SNIPER_SPEED_START;
  dir = 1;
  zoneW = SNIPER_ZONE_START;
  lives = SNIPER_LIVES;
  score = 0;
  level = 1;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  hitUntil = 0;
  hitGood = false;
  placeZone();
}

void SniperGame::placeZone() {
  zoneLo = (int)random(0, LED_COUNT - zoneW + 1);
  zoneHi = zoneLo + zoneW - 1;
}

void SniperGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(120, 0, 0), rgb(60, 0, 0));
    char sub[20];
    snprintf(sub, sizeof sub, "SCORE %d", score);
    Ui::banner(c.dpy, "GAME OVER", sub,
               newRecord ? "NEW RECORD!" : "press to retry");
    return;
  }

  // ---- движение курсора ----
  cursor += dir * speed * c.dt;
  if (cursor >= LED_COUNT - 1.0f) { cursor = LED_COUNT - 1.0f; dir = -1; }
  if (cursor <= 0.0f)             { cursor = 0.0f;             dir =  1; }

  // ---- выстрел ----
  if (in.p1.pressed) {
    bool hit = (cursor >= zoneLo - 0.5f && cursor <= zoneHi + 0.5f);
    hitUntil = c.now + 320;
    hitGood = hit;

    if (hit) {
      // чем ближе к центру мишени, тем больше очков
      float center = (zoneLo + zoneHi) / 2.0f;
      float half = (zoneW / 2.0f) + 0.5f;
      float acc = 1.0f - fabsf(cursor - center) / half;
      if (acc < 0) acc = 0;
      int pts = 10 + (int)(acc * 15.0f) + level;
      score += pts;
      level++;

      if (zoneW > 1 && (level % 2 == 0)) zoneW--;
      speed *= 1.09f;
      placeZone();

      c.out.sfx(acc > 0.75f ? Sfx::LevelUp : Sfx::Pickup);
      c.fx.burst(cursor, acc > 0.75f ? rgb(255, 220, 0) : rgb(0, 255, 120), 8, 18.0f);
      c.out.buzz(30);
    } else {
      lives--;
      c.out.sfx(Sfx::Error);
      c.fx.flash(rgb(180, 0, 0), 220);
      if (lives <= 0) {
        over = true;
        overUntil = c.now + 1200;
        newRecord = store.submit(KEY_BEST, score);
        best = store.best(KEY_BEST);
        c.out.sfx(Sfx::GameOver);
      }
    }
  }

  render(c);
  oled(c);
}

void SniperGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // мишень: края тусклее, центр — белое яблочко
  float center = (zoneLo + zoneHi) / 2.0f;
  float half = (zoneW / 2.0f) + 0.5f;
  for (int i = zoneLo; i <= zoneHi; i++) {
    float acc = 1.0f - fabsf((float)i - center) / half;
    uint8_t b = (uint8_t)(60 + acc * 150.0f);
    L.set(i, colScale(rgb(0, 255, 90), b));
  }
  L.addAA(center, rgb(120, 120, 120));

  // курсор с хвостом по направлению движения
  L.addAA(cursor - dir * 1.0f, rgb(60, 40, 0));
  L.addAA(cursor, rgb(255, 200, 40));

  // короткая подсветка результата выстрела
  if (c.now < hitUntil) {
    Color f = hitGood ? rgb(0, 90, 0) : rgb(90, 0, 0);
    L.addAll(colScale(f, (uint8_t)(((hitUntil - c.now) * 255) / 320)));
  }

  // остаток жизней — тусклые красные точки на дальнем краю
  for (int k = 0; k < lives; k++)
    L.add(LED_COUNT - 1 - k, rgb(40, 0, 0));
}

void SniperGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[24];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "LVL %d   ZONE %d", level, zoneW);
  snprintf(l2, sizeof l2, "BEST %d", best);
  snprintf(right, sizeof right, "L%d", level);

  char hearts[8] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(foot, sizeof foot, "LIVES %s", hearts);

  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

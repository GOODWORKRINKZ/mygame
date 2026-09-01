#include "games.h"
#include <math.h>

// ============================================================
//  HOT BOMB — горячая картошка
// ============================================================
//  Бомба летает по ленте. У каждого игрока своя зона в BOMB_ZONE
//  пикселей у его края. Жать можно только когда бомба в твоей зоне:
//  тогда она разворачивается и летит обратно чуть быстрее.
//  Нажал вхолостую — короткий штраф-ступор. Бомба улетела за твой
//  край — раунд твой соперник забрал. Фитиль догорел — проигрывает
//  тот, на чьей половине бомба оказалась.

static const int Z1_HI = BOMB_ZONE - 1;                 // зона P1: [0..Z1_HI]
static const int Z2_LO = LED_COUNT - BOMB_ZONE;         // зона P2: [Z2_LO..31]

void BombGame::reset() {
  m.reset();
  newRound(millis());
}

void BombGame::newRound(uint32_t now) {
  pos = LED_COUNT / 2.0f;
  vel = (random(2) ? 1.0f : -1.0f) * BOMB_SPEED_START;
  fuseTotal = (uint32_t)random(BOMB_FUSE_MIN, BOMB_FUSE_MAX);
  fuseEnd = now + fuseTotal;
  nextTick = now;
  stun[0] = stun[1] = 0;
  volleys = 0;
}

void BombGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  uint32_t left = (c.now < fuseEnd) ? (fuseEnd - c.now) : 0;

  // ---- тиканье фитиля: чем меньше осталось, тем чаще ----
  if (c.now >= nextTick) {
    float k = (float)left / (float)fuseTotal;          // 1 -> 0
    uint32_t interval = (uint32_t)(90 + k * 500);
    nextTick = c.now + interval;
    c.out.sfx(Sfx::Tick);
    if (k < 0.25f) c.out.buzz(15);
  }

  // ---- фитиль догорел ----
  if (left == 0) {
    int loser = (pos < LED_COUNT / 2.0f) ? 1 : 2;
    c.fx.burst(pos, rgb(255, 120, 0), 16, 30.0f);
    c.fx.flash(rgb(255, 60, 0), 500);
    c.out.sfx(Sfx::Explode);
    c.out.buzzPattern(3, 120, 60);
    m.winRound(loser == 1 ? 2 : 1, c.now, c);
    render(c);
    oled(c);
    return;
  }

  // ---- ввод ----
  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    if (!btn[p]->pressed || c.now < stun[p]) continue;
    bool inZone = (p == 0) ? (pos <= Z1_HI + 0.5f) : (pos >= Z2_LO - 0.5f);
    if (inZone) {
      vel = -vel * BOMB_SPEED_MUL;
      volleys++;
      c.out.tone((uint16_t)(700 + volleys * 45), 45);
      c.out.buzz(35);
      c.fx.burst(pos, p == 0 ? COL_P1 : COL_P2, 5, 16.0f);
    } else {
      stun[p] = c.now + 450;         // промах — короткий штраф
      c.out.sfx(Sfx::Error);
    }
  }

  // ---- полёт ----
  pos += vel * c.dt;
  if (pos < -0.5f) {
    c.fx.flash(COL_P2, 250);
    m.winRound(2, c.now, c);          // P1 не отбил
  } else if (pos > LED_COUNT - 0.5f) {
    c.fx.flash(COL_P1, 250);
    m.winRound(1, c.now, c);          // P2 не отбил
  }

  render(c);
  oled(c);
}

void BombGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  uint32_t left = (c.now < fuseEnd) ? (fuseEnd - c.now) : 0;
  float k = (float)left / (float)fuseTotal;            // 1 -> 0
  uint8_t heat = (uint8_t)((1.0f - k) * 40.0f);        // общий "накал" ленты

  for (int i = 0; i < LED_COUNT; i++) L.set(i, rgb(heat, 0, 0));

  // зоны игроков
  bool in1 = (pos <= Z1_HI + 0.5f);
  bool in2 = (pos >= Z2_LO - 0.5f);
  for (int i = 0; i <= Z1_HI; i++)
    L.add(i, colScale(COL_P1, in1 ? 110 : 26));
  for (int i = Z2_LO; i < LED_COUNT; i++)
    L.add(i, colScale(COL_P2, in2 ? 110 : 26));

  // ступор игрока — его зона мигает красным
  for (int p = 0; p < 2; p++) {
    if (c.now >= stun[p]) continue;
    uint8_t f = ((c.now / 60) & 1) ? 120 : 20;
    if (p == 0) for (int i = 0; i <= Z1_HI; i++)      L.add(i, rgb(f, 0, 0));
    else        for (int i = Z2_LO; i < LED_COUNT; i++) L.add(i, rgb(f, 0, 0));
  }

  // сама бомба: мигает тем быстрее, чем меньше осталось
  uint8_t rate = (uint8_t)(2 + (1.0f - k) * 10.0f);
  uint8_t pulse = sin8((uint8_t)(c.now * rate / 8));
  Color bomb = colLerp(rgb(255, 60, 0), rgb(255, 255, 200), pulse);
  L.addAA(pos, bomb);
  L.addAA(pos - (vel > 0 ? 1.0f : -1.0f), colScale(bomb, 60));
}

void BombGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16], foot[32];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);

  bool in1 = (pos <= Z1_HI + 0.5f);
  bool in2 = (pos >= Z2_LO - 0.5f);
  if (c.now < stun[0])      strcpy(s1, "ОГЛУШЕН");
  else if (in1)             strcpy(s1, "БЕЙ!");
  else                      strcpy(s1, "ЖДИ");
  if (c.now < stun[1])      strcpy(s2, "ОГЛУШЕН");
  else if (in2)             strcpy(s2, "БЕЙ!");
  else                      strcpy(s2, "ЖДИ");

  uint32_t left = (c.now < fuseEnd) ? (fuseEnd - c.now) : 0;
  int fusePct = (int)((left * 100) / fuseTotal);

  Panel l, r;
  // Зелёный (winner=1) — справа, синий (winner=2) — слева.
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.bar = fusePct; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.bar = fusePct; r.active = (m.winner == 1);

  const char* f = foot;
  if (m.over())      f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner) f = "БУМ!";
  else snprintf(foot, sizeof foot, "ЗАПАЛ %lu.%luС  X%d",
                (unsigned long)(left / 1000), (unsigned long)((left % 1000) / 100), volleys);

  drawMatchOled(c, name(), m, l, r, f);
}

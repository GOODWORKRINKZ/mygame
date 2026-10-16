#include "games.h"

// ============================================================
//  FIGHT — бой на двоих
// ============================================================
//  Короткий тап = удар: светлячок летит к сопернику FIGHT_ATTACK_MS,
//  и этого времени ровно хватает, чтобы среагировать.
//  Удержание = блок: он держит удар, но жрёт стамину, а бить из
//  блока нельзя. Ударил в поднятый блок — отлетел в ступор.
//  Продавил пустую стамину — блок ломается и соперник открыт.
//  Отсюда весь смысл: блефовать, выманивать блок и наказывать.

// Дорожка, по которой летит удар: от своей базы до чужой.
static const float LANE_LO = FIGHT_HP + 0.5f;
static const float LANE_HI = LED_COUNT - 1.0f - FIGHT_HP - 0.5f;

void FightGame::reset() {
  m.reset();
  newRound();
}

void FightGame::newRound() {
  for (int p = 0; p < 2; p++) {
    hp[p] = FIGHT_HP;
    stam[p] = FIGHT_STAMINA;
    guard[p] = false;
    stunUntil[p] = 0;
    cd[p] = 0;
    hurtUntil[p] = 0;
  }
  for (auto& s : strikes) s.active = false;
  event = "";
}

void FightGame::launch(int p, Ctx& c) {
  for (int i = 0; i < (int)(sizeof(strikes) / sizeof(strikes[0])); i++) {
    if (strikes[i].active) continue;
    strikes[i].active = true;
    strikes[i].owner = (int8_t)(p + 1);
    strikes[i].start = c.now;
    stam[p] -= FIGHT_ATTACK_COST;
    if (stam[p] < 0) stam[p] = 0;
    cd[p] = c.now + FIGHT_COOLDOWN_MS;
    c.out.tone((uint16_t)(420 + p * 90), 40);
    c.fx.jet(p == 0 ? LANE_LO : LANE_HI, p == 0 ? 1 : -1,
             p == 0 ? COL_P1 : COL_P2, 3, 12.0f);
    return;
  }
}

void FightGame::resolve(int idx, Ctx& c) {
  Strike& s = strikes[idx];
  s.active = false;

  int att = s.owner - 1;
  int def = 1 - att;
  float edge = (def == 0) ? 1.0f : LED_COUNT - 2.0f;

  if (guard[def] && stam[def] > 0.0f) {
    // ---- удар пришёлся в блок ----
    stam[def] -= FIGHT_BLOCK_COST;
    stunUntil[att] = c.now + FIGHT_STUN_MS;
    c.out.sfx(Sfx::Bounce);
    c.out.buzz(25);
    c.fx.burst(edge, rgb(255, 255, 255), 6, 16.0f);

    if (stam[def] <= 0.0f) {
      // блок продавлен: защищавшийся сам уходит в ступор
      stam[def] = 0;
      guard[def] = false;
      stunUntil[def] = c.now + FIGHT_STUN_MS;
      c.out.sfx(Sfx::Error);
      event = "БЛОК СЛОМАН!";
    } else {
      event = "БЛОК!";
    }
    return;
  }

  // ---- чистое попадание ----
  hp[def]--;
  hurtUntil[def] = c.now + 260;
  c.out.sfx(Sfx::Hit);
  c.out.buzz(45);
  c.fx.burst(edge, s.owner == 1 ? COL_P1 : COL_P2, 10, 20.0f);
  event = (s.owner == 1) ? "ЗЕЛЕНЫЙ ПОПАЛ" : "СИНИЙ ПОПАЛ";

  if (hp[def] <= 0) {
    c.fx.flash(s.owner == 1 ? COL_P1 : COL_P2, 300);
    m.winRound(s.owner, c.now, c);
  }
}

void FightGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound();
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  const BtnState* btn[2] = { &in.p1, &in.p2 };

  for (int p = 0; p < 2; p++) {
    bool stunned = (c.now < stunUntil[p]);

    // ---- блок: держим дольше порога, пока есть стамина ----
    bool wantGuard = !stunned && btn[p]->held && btn[p]->heldMs >= FIGHT_GUARD_MS;
    if (wantGuard && stam[p] > 0.0f) {
      if (!guard[p]) c.out.tone((uint16_t)(900 + p * 120), 25);
      guard[p] = true;
      stam[p] -= FIGHT_GUARD_DRAIN * c.dt;
      if (stam[p] <= 0.0f) {
        // стамина кончилась прямо под щитом — короткий ступор
        stam[p] = 0;
        guard[p] = false;
        stunUntil[p] = c.now + FIGHT_STUN_MS / 2;
        c.out.sfx(Sfx::Error);
        event = (p == 0) ? "ЗЕЛЕНЫЙ ВЫДОХСЯ" : "СИНИЙ ВЫДОХСЯ";
      }
    } else {
      guard[p] = false;
      stam[p] += FIGHT_REGEN * c.dt;
      if (stam[p] > FIGHT_STAMINA) stam[p] = FIGHT_STAMINA;
    }

    // ---- удар: короткий тап, решается по отпусканию ----
    if (btn[p]->released && btn[p]->lastHeldMs < FIGHT_GUARD_MS &&
        !stunned && c.now >= cd[p] && stam[p] >= FIGHT_ATTACK_COST) {
      launch(p, c);
    }
  }

  // ---- долетевшие удары ----
  for (int i = 0; i < (int)(sizeof(strikes) / sizeof(strikes[0])); i++) {
    if (!strikes[i].active) continue;
    if (c.now - strikes[i].start >= FIGHT_ATTACK_MS) resolve(i, c);
  }

  render(c);
  oled(c);
}

void FightGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  for (int p = 0; p < 2; p++) {
    Color col = (p == 0) ? COL_P1 : COL_P2;
    bool stunned = (c.now < stunUntil[p]);

    // ---- жизни: столбик у своего края ----
    for (int k = 0; k < FIGHT_HP; k++) {
      int i = (p == 0) ? k : LED_COUNT - 1 - k;
      if (k < hp[p]) {
        uint8_t b = (c.now < hurtUntil[p]) ? 255 : 150;
        L.set(i, colScale(col, b));
      } else {
        L.set(i, rgb(14, 14, 14));
      }
    }

    // ---- щит: яркая точка перед базой, яркость = остаток стамины ----
    float shield = (p == 0) ? LANE_LO : LANE_HI;
    if (guard[p]) {
      uint8_t b = (uint8_t)(90 + 165.0f * (stam[p] / FIGHT_STAMINA));
      L.addAA(shield, colScale(rgb(255, 255, 255), b));
    } else if (stunned) {
      // ступор: край нервно мигает красным
      uint8_t b = ((c.now / 70) & 1) ? 180 : 40;
      L.addAA(shield, colScale(rgb(255, 0, 0), b));
    } else {
      uint8_t b = (uint8_t)(20 + 60.0f * (stam[p] / FIGHT_STAMINA));
      L.addAA(shield, colScale(col, b));
    }
  }

  // ---- летящие удары ----
  for (auto& s : strikes) {
    if (!s.active) continue;
    float t = (float)(c.now - s.start) / (float)FIGHT_ATTACK_MS;
    if (t > 1.0f) t = 1.0f;
    float pos = (s.owner == 1) ? LANE_LO + t * (LANE_HI - LANE_LO)
                               : LANE_HI - t * (LANE_HI - LANE_LO);
    Color col = (s.owner == 1) ? COL_P1 : COL_P2;
    float back = (s.owner == 1) ? -1.0f : 1.0f;
    L.addAA(pos + back, colScale(col, 60));
    L.addAA(pos, rgb(255, 255, 255));
  }
}

void FightGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16];
  snprintf(b1, sizeof b1, "%d", hp[0]);
  snprintf(b2, sizeof b2, "%d", hp[1]);
  snprintf(s1, sizeof s1, "ПОБЕД %d", m.score[0]);
  snprintf(s2, sizeof s2, "ПОБЕД %d", m.score[1]);

  int st1 = (int)((stam[0] / FIGHT_STAMINA) * 100.0f);
  int st2 = (int)((stam[1] / FIGHT_STAMINA) * 100.0f);

  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.bar = st2;
  l.active = guard[1] || (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.bar = st1;
  r.active = guard[0] || (m.winner == 1);

  const char* foot = hint();
  if (m.over())            foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner)       foot = (m.winner == 1) ? "ЗЕЛЕНЫЙ ВЗЯЛ РАУНД" : "СИНИЙ ВЗЯЛ РАУНД";
  else if (event[0])       foot = event;

  drawMatchOled(c, name(), m, l, r, foot);
}

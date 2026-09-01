#include "games.h"
#include <math.h>

// ============================================================
//  SHOOTER — дуэль баз
// ============================================================
//  База игрока = его HP, выложенные светодиодами от своего края.
//  Короткое нажатие — обычный снаряд (1 урон).
//  Удержание CHARGE_FULL_MS и отпускание — заряженный (2 урона,
//  быстрее, пробивает обычный вражеский снаряд насквозь).
//  Встречные снаряды одинаковой силы уничтожают друг друга.

static const int MAX_SHOTS_PER_PLAYER = 3;

void ShooterGame::reset() {
  m.reset();
  newRound();
}

void ShooterGame::newRound() {
  hp[0] = hp[1] = SHOOTER_HP;
  for (int i = 0; i < SHOT_MAX; i++) shots[i].active = false;
  cd[0] = cd[1] = 0;
  chargeAnnounced[0] = chargeAnnounced[1] = false;
}

void ShooterGame::fire(int player, bool charged, Ctx& c) {
  int slot = -1, mine = 0;
  for (int i = 0; i < SHOT_MAX; i++) {
    if (shots[i].active && shots[i].owner == player) mine++;
    else if (!shots[i].active && slot < 0) slot = i;
  }
  if (slot < 0 || mine >= MAX_SHOTS_PER_PLAYER) { c.out.sfx(Sfx::Click); return; }

  Shot& s = shots[slot];
  s.active = true;
  s.owner  = (int8_t)player;
  s.power  = charged ? 2 : 1;
  s.pos    = (player == 1) ? (float)hp[0] : (float)(LED_COUNT - 1 - hp[1]);
  s.vel    = (charged ? SHOT_SPEED_CHARGED : SHOT_SPEED) * (player == 1 ? 1.0f : -1.0f);

  cd[player - 1] = c.now + SHOT_COOLDOWN_MS;
  c.out.sfx(charged ? Sfx::ShootCharged : Sfx::Shoot);
  c.fx.jet(s.pos, player == 1 ? -1 : 1,
           charged ? rgb(255, 150, 0) : rgb(180, 180, 180), 3, 9.0f);
}

void ShooterGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound();
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c, in);
      return;
    }
  }

  // ---- ввод: выстрел происходит по отпусканию кнопки ----
  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    const BtnState& b = *btn[p];
    if (b.held && b.heldMs >= CHARGE_FULL_MS && !chargeAnnounced[p]) {
      chargeAnnounced[p] = true;
      c.out.tone(2000, 45);          // "заряд готов"
      c.out.buzz(30);
    }
    if (b.released) {
      if (c.now >= cd[p]) fire(p + 1, b.lastHeldMs >= CHARGE_FULL_MS, c);
      chargeAnnounced[p] = false;
    }
  }

  // ---- движение снарядов ----
  for (int i = 0; i < SHOT_MAX; i++) {
    if (!shots[i].active) continue;
    shots[i].pos += shots[i].vel * c.dt;
    if (shots[i].pos < -1.0f || shots[i].pos > LED_COUNT) shots[i].active = false;
  }

  // ---- лобовые столкновения ----
  // Снаряды P1 всегда стартуют левее снарядов P2 и летят навстречу,
  // поэтому достаточно проверить, что левый догнал правого.
  for (int i = 0; i < SHOT_MAX; i++) {
    if (!shots[i].active || shots[i].owner != 1) continue;
    for (int j = 0; j < SHOT_MAX; j++) {
      if (!shots[j].active || shots[j].owner != 2) continue;
      Shot& a = shots[i];
      Shot& b = shots[j];
      if (a.pos < b.pos - 0.6f) continue;

      float at = (a.pos + b.pos) * 0.5f;
      if (a.power == b.power) {
        a.active = b.active = false;
      } else if (a.power > b.power) {
        a.power--; b.active = false;
      } else {
        b.power--; a.active = false;
      }
      c.fx.burst(at, rgb(255, 170, 40), 8, 16.0f);
      c.out.sfx(Sfx::Explode);
      if (!a.active) break;
    }
  }

  // ---- попадания по базам ----
  for (int i = 0; i < SHOT_MAX; i++) {
    Shot& s = shots[i];
    if (!s.active) continue;

    int victim = -1;
    if (s.owner == 1 && s.pos >= (float)(LED_COUNT - hp[1]) - 0.5f) victim = 1;
    if (s.owner == 2 && s.pos <= (float)hp[0] - 0.5f)                victim = 0;
    if (victim < 0) continue;

    hp[victim] -= s.power;
    if (hp[victim] < 0) hp[victim] = 0;
    s.active = false;

    c.fx.burst(s.pos, victim == 0 ? COL_P1 : COL_P2, 10, 18.0f);
    c.fx.flash(rgb(120, 0, 0), 120);
    c.out.sfx(Sfx::Hit);

    if (hp[victim] == 0) {
      c.fx.shock(victim == 0 ? 0.0f : LED_COUNT - 1.0f, rgb(255, 200, 0), 45.0f, 500);
      m.winRound(victim == 0 ? 2 : 1, c.now, c);
      break;
    }
  }

  render(c, in);
  oled(c, in);
}

void ShooterGame::render(Ctx& c, const Inputs& in) {
  LedStrip& L = c.leds;
  L.clear();

  const BtnState* btn[2] = { &in.p1, &in.p2 };

  for (int p = 0; p < 2; p++) {
    Color base = (p == 0) ? COL_P1 : COL_P2;
    // пока копится заряд, база пульсирует и белеет
    float ch = 0;
    if (btn[p]->held) {
      ch = (float)btn[p]->heldMs / (float)CHARGE_FULL_MS;
      if (ch > 1.0f) ch = 1.0f;
    }
    uint8_t pulse = (ch >= 1.0f) ? sin8((uint8_t)(c.now / 3)) : 255;

    for (int k = 0; k < hp[p]; k++) {
      int idx = (p == 0) ? k : (LED_COUNT - 1 - k);
      // внешний край базы ярче — видно "дуло"
      uint8_t b = (uint8_t)(120 + (135 * (hp[p] - k)) / hp[p]);
      Color col = colLerp(base, rgb(255, 255, 255), (uint8_t)(ch * 200.0f));
      L.set(idx, colScale(col, (uint8_t)(((uint16_t)b * (160 + pulse / 3)) >> 8)));
    }
  }

  for (int i = 0; i < SHOT_MAX; i++) {
    if (!shots[i].active) continue;
    Color col = (shots[i].power >= 2) ? rgb(255, 140, 0) : rgb(255, 255, 255);
    L.addAA(shots[i].pos, col);
    // короткий хвост позади снаряда
    float back = shots[i].pos - (shots[i].vel > 0 ? 0.9f : -0.9f);
    L.addAA(back, colScale(col, 70));
  }
}

void ShooterGame::oled(Ctx& c, const Inputs& in) {
  char b1[8], b2[8], s1[12], s2[12];
  snprintf(b1, sizeof b1, "%d", hp[0]);
  snprintf(b2, sizeof b2, "%d", hp[1]);

  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    char* dst = (p == 0) ? s1 : s2;
    if (btn[p]->held && btn[p]->heldMs >= CHARGE_FULL_MS) strcpy(dst, "CHARGED!");
    else if (btn[p]->held)                                strcpy(dst, "charging");
    else                                                  strcpy(dst, "HP");
  }

  Panel l, r;
  l.name = "GREEN"; l.big = b1; l.sub = s1;
  l.dots = m.score[0]; l.dotsMax = ROUNDS_TO_WIN; l.active = (m.winner == 1);
  r.name = "BLUE";  r.big = b2; r.sub = s2;
  r.dots = m.score[1]; r.dotsMax = ROUNDS_TO_WIN; r.active = (m.winner == 2);

  const char* foot = hint();
  if (m.over())      foot = (m.winner == 1) ? "GREEN WINS THE MATCH" : "BLUE WINS THE MATCH";
  else if (m.winner) foot = "base destroyed!";

  drawMatchOled(c, name(), m, l, r, foot);
}

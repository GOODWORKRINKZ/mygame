#include "game.h"
#include <Preferences.h>

// ============================================================
//  Match
// ============================================================

void Match::reset() {
  score[0] = score[1] = 0;
  round = 1;
  winner = 0;
  phase = MatchPhase::Playing;
  until = 0;
}

void Match::winRound(int p, uint32_t now, Ctx& c) {
  if (p != 1 && p != 2) return;
  score[p - 1]++;
  winner = p;
  Color col = (p == 1) ? COL_P1 : COL_P2;

  if (score[p - 1] >= ROUNDS_TO_WIN) {
    phase = MatchPhase::MatchOver;
    until = now + 4000;
    c.out.sfx(Sfx::Fanfare);
    c.fx.flash(col, 400);
  } else {
    phase = MatchPhase::RoundOver;
    until = now + 1500;
    c.out.sfx(Sfx::Win);
    c.fx.flash(col, 250);
  }
}

bool Match::tick(uint32_t now) {
  if (phase == MatchPhase::Playing) return false;
  if (now < until) return false;

  if (phase == MatchPhase::MatchOver) {
    reset();
  } else {
    round++;
    winner = 0;
    phase = MatchPhase::Playing;
  }
  return true;
}

// ============================================================
//  Общие экраны
// ============================================================

void drawCelebration(Ctx& c, int winner, bool matchOver) {
  Color col = (winner == 1) ? COL_P1 : COL_P2;

  if (matchOver) {
    // Фейерверк: конфетти цветов победителя + случайные искры.
    c.leds.fade(180);
    if (random(100) < 60)
      c.leds.add((int)random(LED_COUNT), colScale(col, (uint8_t)random(120, 256)));
    if (random(100) < 25)
      c.fx.spark((float)random(LED_COUNT), (float)random(-20, 20), rgb(255, 255, 255), 0.4f);
  } else {
    // Раунд: волна цвета победителя от его края.
    uint8_t phase = (uint8_t)((c.now / 4) & 0xFF);
    for (int i = 0; i < LED_COUNT; i++) {
      int d = (winner == 1) ? i : (LED_COUNT - 1 - i);
      uint8_t b = sin8((uint8_t)(phase - d * 8));
      c.leds.set(i, colScale(col, (uint8_t)(40 + (uint16_t)b * 200 / 255)));
    }
  }
}

void drawMatchOled(Ctx& c, const char* title, const Match& m,
                   const Panel& left, const Panel& right, const char* foot) {
  char rnd[10];
  snprintf(rnd, sizeof rnd, "Р%d", m.round);
  Ui::split(c.dpy, title, rnd, left, right, foot);
}

// ============================================================
//  Store — рекорды в NVS
// ============================================================

static Preferences g_prefs;
static bool g_prefsOk = false;

Store store;

void Store::begin() {
  g_prefsOk = g_prefs.begin("arcade", false);
  if (!g_prefsOk) LOG_W("NVS", "preferences open failed, records will not persist");
  else            LOG_I("NVS", "records storage ready");
}

int Store::best(const char* key) {
  if (!g_prefsOk) return 0;
  return g_prefs.getInt(key, 0);
}

bool Store::submit(const char* key, int value) {
  if (value <= best(key)) return false;
  if (g_prefsOk) g_prefs.putInt(key, value);
  LOG_I("NVS", "new record %s = %d", key, value);
  return true;
}

void Store::wipe() {
  if (g_prefsOk) g_prefs.clear();
  LOG_I("NVS", "records wiped");
}

const char* playersLabel(uint8_t n) {
  return n == 1 ? "1И" : "2И";
}

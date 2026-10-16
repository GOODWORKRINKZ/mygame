#include "games.h"
#include "sprites.h"
#include <math.h>

// ============================================================
//  RACE — гонка на двоих (матрица 4x4 + 3D сплит-экран)
// ============================================================
//  Четырёхполосное шоссе. Синий едет по левой паре полос (0-1),
//  зелёный — по правой (2-3); кнопка переключает машину между своими
//  двумя полосами. Сверху идёт встречный трафик, в ряду не больше одной
//  машины на пару полос — увернуться можно всегда, вопрос реакции.
//  Задел — минус жизнь, проехал — очко. Раунд до тех пор, пока у
//  кого-то не кончатся жизни; матч до 3 побед.
//
//  На OLED синяя зона поделена пополам, каждая половина повёрнута к
//  своему игроку (как сплит-табло): в ней его дорога в перспективе,
//  встречные машины растут по глубине, своя машина сзади, соперник
//  рядом. Жёлтая полоса — общее табло.

static const int   N = MATRIX_SIZE;
static const float FLOOR_Y = (float)(N - 1);

static inline int mcol(int x) { return MATRIX_COL0 + x; }
// p = 0 зелёный (полосы 2-3), p = 1 синий (полосы 0-1)
static inline int laneOf(int p, int idx) { return (p == 0) ? 2 + idx : idx; }

void RaceGame::reset() {
  m.reset();
  newRound(millis());
}

void RaceGame::newRound(uint32_t now) {
  for (auto& t : traffic) t.active = false;
  laneIdx[0] = 0;                 // зелёный — внутренняя полоса 2
  laneIdx[1] = 1;                 // синий — внутренняя полоса 1
  laneX[0] = (float)laneOf(0, laneIdx[0]);
  laneX[1] = (float)laneOf(1, laneIdx[1]);
  lives[0] = lives[1] = RACE_LIVES;
  dodged[0] = dodged[1] = 0;
  hurtUntil[0] = hurtUntil[1] = 0;
  speed = RACE_SPEED_START;
  spawnIn = 1.5f;
  dist = 0;
  rows = 0;
  startAt = now + 1400;
}

// Ряд трафика: на каждую пару полос не больше одной машины.
void RaceGame::spawnRow() {
  int p = 55 + rows * 2;
  if (p > 85) p = 85;
  bool any = false;
  for (int tries = 0; tries < 3 && !any; tries++) {
    for (int pair = 0; pair < 2; pair++) {
      if ((int)random(100) >= p) continue;
      for (auto& t : traffic) {
        if (t.active) continue;
        t.active = true;
        t.judged = false;
        t.col = (uint8_t)(pair * 2 + random(2));
        t.y = -1.0f;
        any = true;
        break;
      }
    }
  }
  rows++;
}

void RaceGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
      c.out.sfx(Sfx::Countdown);
    } else {
      drawCelebration(c, m.winner, m.over());
      oledMatch(c);
      return;
    }
  }

  // ---- переключение полосы ----
  if (in.p1.pressed) { laneIdx[0] ^= 1; c.out.sfx(Sfx::Click); }
  if (in.p2.pressed) { laneIdx[1] ^= 1; c.out.sfx(Sfx::Click); }
  for (int p = 0; p < 2; p++) {
    float target = (float)laneOf(p, laneIdx[p]);
    float d = target - laneX[p];
    float st = 10.0f * c.dt;
    if (fabsf(d) <= st) laneX[p] = target; else laneX[p] += (d > 0 ? st : -st);
  }

  // ---- отсчёт ----
  if (startAt) {
    if (c.now >= startAt) startAt = 0;
    render(c);
    oled(c);
    return;
  }

  dist += speed * c.dt;
  spawnIn -= speed * c.dt;
  if (spawnIn <= 0) {
    spawnRow();
    float gap = 2.2f - (dodged[0] + dodged[1]) * 0.01f;
    if (gap < RACE_GAP_MIN_ROWS) gap = RACE_GAP_MIN_ROWS;
    spawnIn = gap * ((float)random(85, 120) / 100.0f);
  }

  // ---- трафик ----
  for (auto& t : traffic) {
    if (!t.active) continue;
    t.y += speed * c.dt;
    if (!t.judged && t.y >= FLOOR_Y - 0.5f) {
      t.judged = true;
      int p = (t.col < 2) ? 1 : 0;
      if ((int)t.col == laneOf(p, laneIdx[p]) && c.now >= hurtUntil[p]) {
        lives[p]--;
        hurtUntil[p] = c.now + 1000;
        c.out.sfx(Sfx::Error);
        c.out.buzzPattern(2, 60, 50);
        c.fx.burst(2.0f, p == 0 ? COL_P1 : COL_P2, 8, 14.0f, p == 0 ? &ARM_P1 : &ARM_P2);
      } else {
        dodged[p]++;
        speed += RACE_SPEED_UP;
        if (speed > RACE_SPEED_MAX) speed = RACE_SPEED_MAX;
      }
    }
    if (t.y > FLOOR_Y + 1.0f) t.active = false;
  }

  // ---- конец раунда ----
  if (lives[0] <= 0 || lives[1] <= 0) {
    int winner;
    if (lives[0] <= 0 && lives[1] <= 0) winner = (dodged[0] >= dodged[1]) ? 1 : 2;
    else                                winner = (lives[0] <= 0) ? 2 : 1;
    c.fx.flash(winner == 1 ? COL_P1 : COL_P2, 250);
    m.winRound(winner, c.now, c);
  }

  render(c);
  oled(c);
}

// ------------------------------------------------------------
//  Лента: вид сверху
// ------------------------------------------------------------
void RaceGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // асфальт, половины чуть подкрашены цветом хозяина; разметка бежит вниз
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      Color base = (x < 2) ? rgb(1, 2, 8) : rgb(1, 8, 2);
      int k = (int)(dist * 2.0f) + y * 2 + (x & 1) * 5;
      if ((k % 7) == 0) base = rgb(12, 12, 14);
      L.setXY(mcol(x), y, base);
    }

  for (auto& t : traffic) {
    if (!t.active) continue;
    L.addAAXY((float)mcol(t.col), t.y, rgb(255, 200, 40));
  }

  for (int p = 0; p < 2; p++) {
    Color me = (p == 0) ? COL_P1 : COL_P2;
    if (c.now < hurtUntil[p]) me = ((c.now / 70) & 1) ? rgb(255, 0, 0) : rgb(60, 0, 0);
    L.addAAXY((float)MATRIX_COL0 + laneX[p], FLOOR_Y, me);
  }

  // рукава: жизни (3 точки у кнопки) и очки
  for (int p = 0; p < 2; p++) {
    const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    for (int i = 0; i < 3; i++)
      L.set(arm, i, (i < lives[p]) ? colScale(rgb(255, 40, 40), 140) : rgb(6, 2, 2));
    int bar = dodged[p] / 3;
    for (int i = 3; i < arm.len; i++)
      L.set(arm, i, (i - 3 < bar) ? colScale(col, 120) : colScale(col, 8));
  }

  if (startAt) {
    uint32_t left = (startAt > c.now) ? startAt - c.now : 0;
    uint8_t b = (uint8_t)(40 + (left % 400) * 120 / 400);
    L.fill(CORE, colScale(rgb(60, 60, 60), b));
  }
}

// ------------------------------------------------------------
//  OLED: две дороги, каждая повёрнута к своему игроку
// ------------------------------------------------------------
static const int PW = 46, PH = 63;     // панель в повёрнутых координатах
static const int P_HORIZON = 17;

static inline int pLaneX(float lane, float t) {
  return (int)(23.0f + (lane - 1.5f) * (3.0f + 8.0f * t) + 0.5f);
}
static inline int pDepthY(float t) {
  return (int)(P_HORIZON + t * t * (PH - 1 - P_HORIZON) + 0.5f);
}

void RaceGame::drawRoad(Adafruit_SSD1306& g, int rot, int me, uint32_t now) {
  g.setRotation(rot);
  const int x0 = (rot == 1) ? 18 : 0;
  const int y0 = 65;
  auto X = [&](int px) { return x0 + px; };
  auto Y = [&](int py) { return y0 + py; };

  // горизонт и края дороги
  g.drawFastHLine(X(0), Y(P_HORIZON), PW, SSD1306_WHITE);
  g.drawLine(X(pLaneX(-0.5f, 0)), Y(P_HORIZON), X(pLaneX(-0.5f, 1)), Y(PH - 1), SSD1306_WHITE);
  g.drawLine(X(pLaneX(3.5f, 0)),  Y(P_HORIZON), X(pLaneX(3.5f, 1)),  Y(PH - 1), SSD1306_WHITE);
  // разметка: средняя линия чаще, боковые реже
  for (int l = 1; l < N; l++) {
    int n = (l == 2) ? 7 : 5;
    for (int k = 0; k < n; k++) {
      float t = fmodf((float)k / n + dist * 0.2f, 1.0f);
      g.drawPixel(X(pLaneX((float)l - 0.5f, t)), Y(pDepthY(t)), SSD1306_WHITE);
    }
  }

  // встречные: дальние первыми
  for (int pass = 0; pass < 2; pass++) {
    for (auto& tr : traffic) {
      if (!tr.active) continue;
      float t = (tr.y + 1.0f) / (float)N;
      if (t < 0) t = 0;
      if (t > 1.1f) continue;
      bool far = t < 0.7f;
      if ((pass == 0) != far) continue;
      float tc = t > 1.0f ? 1.0f : t;
      int x = X(pLaneX((float)tr.col, tc)), yb = Y(pDepthY(tc));
      if (tc < 0.35f)      g.drawPixel(x, yb, SSD1306_WHITE);
      else if (tc < 0.7f)  g.drawBitmap(x - 2, yb - 3, SPR_CAR_FRONT_S, 4, 3, SSD1306_WHITE);
      else                 g.drawBitmap(x - 4, yb - 6, SPR_CAR_FRONT_L, 8, 6, SSD1306_WHITE);
    }
  }

  // машины игроков у нижнего края: своя и соперника
  for (int p = 0; p < 2; p++) {
    bool blink = (now < hurtUntil[p]) && ((now / 90) & 1);
    if (blink) continue;
    int x = X(pLaneX(laneX[p], 1.0f));
    g.drawBitmap(x - 5, Y(PH - 8), SPR_CAR_REAR, 10, 7, SSD1306_WHITE);
    if (p == me) g.drawPixel(x, Y(PH - 10), SSD1306_WHITE);   // метка "это ты"
  }

  g.setRotation(0);
}

void RaceGame::oled(Ctx& c) {
  Display& d = c.dpy;
  Adafruit_SSD1306& g = d.gfx();
  d.clear();

  // жёлтая полоса: табло
  char l1[26], l2[26];
  snprintf(l1, sizeof l1, "ГОНКА  Р%d", m.round);
  d.text(0, 0, l1, 1);
  char h1[6] = "", h2[6] = "";
  for (int i = 0; i < lives[1] && i < 3; i++) strcat(h1, "*");
  for (int i = 0; i < lives[0] && i < 3; i++) strcat(h2, "*");
  if (startAt) snprintf(l2, sizeof l2, "ПРИГОТОВЬСЯ...");
  else         snprintf(l2, sizeof l2, "С%-3s %-3d  З%-3s %d", h1, dodged[1], h2, dodged[0]);
  d.textCentered(8, l2, 1);
  g.drawFastVLine(63, 18, 46, SSD1306_WHITE);

  drawRoad(g, 1, 1, c.now);    // левая половина — синий
  drawRoad(g, 3, 0, c.now);    // правая половина — зелёный
  d.flush();
}

void RaceGame::oledMatch(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);
  snprintf(s1, sizeof s1, "ПРОЕХАЛ %d", dodged[0]);
  snprintf(s2, sizeof s2, "ПРОЕХАЛ %d", dodged[1]);
  Panel l, r;
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2; l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (m.winner == 2);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1; r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (m.winner == 1);
  const char* f = m.over() ? ((m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ")
                           : ((m.winner == 1) ? "ЗЕЛЕНЫЙ ДОЕХАЛ" : "СИНИЙ ДОЕХАЛ");
  drawMatchOled(c, name(), m, l, r, f);
}

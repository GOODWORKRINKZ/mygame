#include "games.h"
#include "sprites.h"
#include <math.h>

// ============================================================
//  TUNNEL — туннель (1 игрок, матрица 4x4 + вид из кабины на OLED)
// ============================================================
//  Корабль летит по туннелю. На матрице вид сверху: корабль точкой
//  внизу, астероиды падают по колонкам. ЗЕЛЁНАЯ/СИНЯЯ — шаг вправо/
//  влево. ОБЕ КНОПКИ почти одновременно — выстрел: снаряд уходит в
//  глубину по своей полосе и разбивает первый астероид. Чтобы выстрел
//  не превращался в случайный шаг, одиночное нажатие исполняется с
//  задержкой TUNNEL_FIRE_WINDOW_MS — если за это время пришла вторая
//  кнопка, это был выстрел.
//  На OLED — вид из кабины: кольца туннеля летят на зрителя из точки
//  схода, астероиды растут из глубины по своей полосе, снаряд уходит
//  вдаль, внизу нос корабля и прицел на текущей полосе.

static const char* KEY_BEST = "tunnel";
static const int   N = MATRIX_SIZE;
static const float FLOOR_Y = (float)(N - 1);

static inline int mcol(int x) { return MATRIX_COL0 + x; }

void TunnelGame::reset() {
  for (auto& o : obs) o.active = false;
  shot.active = false;
  lane = 1;
  laneX = 1.0f;
  speed = TUNNEL_SPEED_START;
  spawnIn = 1.4f;
  lives = TUNNEL_LIVES;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  hurtUntil = 0;
  dist = 0;
  lastLane = -1;
  pendingDir = 0;
  pendingAt = 0;
}

// Достижимые полосы к новому ряду (см. ДИНО): не больше одной полосы
// сдвига между соседними рядами, занятые полосы выпадают.
uint8_t TunnelGame::reachableLanes() const {
  struct Row { float y; uint8_t mask; };
  Row rows[MAX_OBS];
  int nr = 0;
  for (auto& o : obs) {
    if (!o.active || o.judged) continue;
    int k = -1;
    for (int i = 0; i < nr; i++) if (fabsf(rows[i].y - o.y) < 0.4f) { k = i; break; }
    if (k < 0) { rows[nr].y = o.y; rows[nr].mask = 0; k = nr++; }
    rows[k].mask |= (uint8_t)(1 << o.col);
  }
  for (int i = 1; i < nr; i++)
    for (int j = i; j > 0 && rows[j].y > rows[j - 1].y; j--) { Row t = rows[j]; rows[j] = rows[j - 1]; rows[j - 1] = t; }
  uint8_t reach = (uint8_t)(1 << lane);
  for (int i = 0; i < nr; i++) {
    uint8_t grown = (uint8_t)((reach | (reach << 1) | (reach >> 1)) & 0x0F);
    reach = (uint8_t)(grown & ~rows[i].mask);
    if (!reach) return 0;
  }
  return (uint8_t)((reach | (reach << 1) | (reach >> 1)) & 0x0F);
}

void TunnelGame::spawnRow() {
  uint8_t reach = reachableLanes();
  if (reach == 0) reach = 0x0F;
  int n = (score >= 8 && random(100) < 40) ? 2 : 1;
  int c1 = -1, c2 = -1;
  for (int tries = 0; tries < 12; tries++) {
    c1 = (int)random(0, N);
    if (c1 == lastLane && random(2)) c1 = (c1 + 1) % N;
    c2 = -1;
    if (n == 2) { c2 = (int)random(0, N); if (c2 == c1) c2 = (c1 + 2) % N; }
    uint8_t blocked = (uint8_t)((1 << c1) | (c2 >= 0 ? (1 << c2) : 0));
    if (reach & ~blocked) break;
    if (tries >= 6) n = 1;
    c1 = -1;
  }
  if (c1 < 0) {
    for (int c = 0; c < N; c++) if (!(reach & (1 << c))) { c1 = c; break; }
    if (c1 < 0) return;
    c2 = -1;
  }
  lastLane = c1;
  for (auto& o : obs) {
    if (o.active) continue;
    if (c1 >= 0)      { o.active = true; o.col = (uint8_t)c1; o.y = -1.0f; o.judged = false; o.big = random(100) < 30; c1 = -1; }
    else if (c2 >= 0) { o.active = true; o.col = (uint8_t)c2; o.y = -1.0f; o.judged = false; o.big = random(100) < 30; c2 = -1; }
    if (c1 < 0 && c2 < 0) break;
  }
}

void TunnelGame::fire(Ctx& c) {
  if (shot.active) return;
  shot.active = true;
  shot.col = (uint8_t)lane;
  shot.y = FLOOR_Y;
  c.out.sfx(Sfx::Shoot);
}

void TunnelGame::update(const Inputs& in, Ctx& c) {
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(40, 40, 120), rgb(10, 10, 40));
    char sub[22];
    snprintf(sub, sizeof sub, "ОЧКИ %d", score);
    Ui::banner(c.dpy, "РАЗБИЛСЯ", sub, newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- ввод: две кнопки в окне = выстрел, иначе отложенный шаг ----
  if (in.p1.pressed && in.p2.pressed) {
    pendingDir = 0; fire(c);
  } else if (in.p1.pressed || in.p2.pressed) {
    int d = in.p1.pressed ? +1 : -1;
    uint32_t at = in.p1.pressed ? in.p1.pressedAt : in.p2.pressedAt;
    if (pendingDir && pendingDir != d && at - pendingAt <= TUNNEL_FIRE_WINDOW_MS) {
      pendingDir = 0; fire(c);
    } else {
      pendingDir = d; pendingAt = at;
    }
  }
  if (pendingDir && c.now - pendingAt > TUNNEL_FIRE_WINDOW_MS) {
    lane += pendingDir;
    if (lane < 0) lane = 0;
    if (lane > N - 1) lane = N - 1;
    pendingDir = 0;
    c.out.sfx(Sfx::Click);
  }
  float dxl = (float)lane - laneX;
  float stepL = 9.0f * c.dt;
  if (fabsf(dxl) <= stepL) laneX = (float)lane; else laneX += (dxl > 0 ? stepL : -stepL);

  dist += speed * c.dt;
  spawnIn -= speed * c.dt;
  if (spawnIn <= 0) {
    spawnRow();
    float gapRows = 2.0f - score * 0.015f;
    if (gapRows < TUNNEL_GAP_MIN_ROWS) gapRows = TUNNEL_GAP_MIN_ROWS;
    spawnIn = gapRows * ((float)random(85, 120) / 100.0f);
  }

  // ---- снаряд ----
  if (shot.active) {
    shot.y -= TUNNEL_SHOT_SPEED * c.dt;
    for (auto& o : obs) {
      if (!o.active || o.col != shot.col || fabsf(o.y - shot.y) > 0.6f) continue;
      o.active = false;
      shot.active = false;
      score += 3;
      c.out.sfx(Sfx::Explode);
      c.out.buzz(20);
      break;
    }
    if (shot.y < -1.5f) shot.active = false;
  }

  // ---- астероиды ----
  for (auto& o : obs) {
    if (!o.active) continue;
    o.y += speed * c.dt;
    if (!o.judged && o.y >= FLOOR_Y - 0.5f) {
      o.judged = true;
      if ((int)o.col == lane && c.now >= hurtUntil) {
        lives--;
        hurtUntil = c.now + 1000;
        c.out.sfx(Sfx::Error);
        c.out.buzzPattern(2, 70, 60);
        c.fx.flash(rgb(160, 0, 0), 220);
        if (lives <= 0) {
          over = true; overUntil = c.now + 1200;
          newRecord = store.submit(KEY_BEST, score);
          best = store.best(KEY_BEST);
          c.out.sfx(Sfx::GameOver);
          return;
        }
      } else {
        score++;
        speed += TUNNEL_SPEED_UP;
        if (speed > TUNNEL_SPEED_MAX) speed = TUNNEL_SPEED_MAX;
      }
    }
    if (o.y > FLOOR_Y + 1.0f) o.active = false;
  }

  render(c);
  oled(c);
}

// ------------------------------------------------------------
//  Лента: вид сверху
// ------------------------------------------------------------
void TunnelGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      int k = (int)(dist * 3.0f) + y * 5 + x * 3;
      L.setXY(mcol(x), y, (k % 9) == 0 ? rgb(10, 10, 20) : rgb(1, 1, 4));
    }
  for (auto& o : obs) {
    if (!o.active) continue;
    L.addAAXY((float)mcol(o.col), o.y, o.big ? rgb(255, 120, 40) : rgb(180, 180, 200));
  }
  if (shot.active) L.addAAXY((float)mcol(shot.col), shot.y, rgb(80, 255, 120));
  Color me = rgb(120, 220, 255);
  if (c.now < hurtUntil) me = ((c.now / 70) & 1) ? rgb(255, 0, 0) : rgb(80, 0, 0);
  L.addAAXY((float)MATRIX_COL0 + laneX, FLOOR_Y, me);

  for (int i = 0; i < ARM_P1.len; i++)
    L.set(ARM_P1, i, (i < lives) ? colScale(rgb(255, 40, 40), 140) : rgb(6, 2, 2));
  float k = (speed - TUNNEL_SPEED_START) / (TUNNEL_SPEED_MAX - TUNNEL_SPEED_START);
  float lit = k * ARM_P2.len;
  for (int i = 0; i < ARM_P2.len; i++) {
    float f = lit - i;
    uint8_t b = f >= 1.0f ? 120 : (f > 0 ? (uint8_t)(f * 120) : 0);
    L.set(ARM_P2, i, colScale(rgb(80, 160, 255), (uint8_t)(8 + b)));
  }
}

// ------------------------------------------------------------
//  OLED: вид из кабины
// ------------------------------------------------------------
static const int VX = 64, VY = 40;          // точка схода

static inline int tLaneX(float lane, float t) {
  return (int)(VX + (lane - 1.5f) * (4.0f + 28.0f * t * t) + 0.5f);
}
static inline int tDepthY(float t) { return (int)(VY + 14.0f * t * t + 0.5f); }

void TunnelGame::oled(Ctx& c) {
  Display& d = c.dpy;
  Adafruit_SSD1306& g = d.gfx();
  d.clear();

  char top[26];
  snprintf(top, sizeof top, "ТУННЕЛЬ  %d", score);
  d.text(0, 1, top, 1);
  char right[14], hearts[6] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(right, sizeof right, "%s  Р%d", hearts, best);
  d.text(OLED_WIDTH - Display::textWidth(right, 1), 1, right, 1);
  g.drawFastHLine(0, 15, OLED_WIDTH, SSD1306_WHITE);

  // кольца туннеля летят на зрителя
  for (int k = 0; k < 7; k++) {
    float t = fmodf((float)k / 7.0f + dist * 0.12f, 1.0f);
    float q = t * t;
    int hw = (int)(3 + 61 * q), hh = (int)(2 + 20 * q);
    int x0 = VX - hw, y0 = VY - hh, w = 2 * hw, h = 2 * hh;
    if (y0 < 18) { h -= (18 - y0); y0 = 18; }
    if (y0 + h > 63) h = 63 - y0;
    if (t > 0.15f) g.drawRect(x0, y0, w, h, SSD1306_WHITE);
    else           g.drawPixel(VX, VY, SSD1306_WHITE);
  }
  // звёзды в глубине — редкие пиксели, расходящиеся от центра
  for (int s = 0; s < 10; s++) {
    float a = s * 0.63f;
    float r = fmodf(s * 0.37f + dist * 0.05f, 1.0f);
    int x = VX + (int)(cosf(a) * r * 62), y = VY + (int)(sinf(a) * r * 22);
    if (y >= 18 && y <= 63) g.drawPixel(x, y, SSD1306_WHITE);
  }

  // астероиды, дальние первыми
  for (int pass = 0; pass < 2; pass++) {
    for (auto& o : obs) {
      if (!o.active) continue;
      float t = (o.y + 1.0f) / (float)N;
      if (t < 0) t = 0;
      if (t > 1.1f) continue;
      bool far = t < 0.65f;
      if ((pass == 0) != far) continue;
      float tc = t > 1.0f ? 1.0f : t;
      int x = tLaneX((float)o.col, tc), y = tDepthY(tc);
      if (tc < 0.35f)      g.fillRect(x - 1, y - 1, 2, 2, SSD1306_WHITE);
      else if (tc < 0.65f) g.drawBitmap(x - 4, y - 3, SPR_ASTEROID_S, 8, 6, SSD1306_WHITE);
      else {
        g.drawBitmap(x - 8, y - 6, SPR_ASTEROID_L, 16, 12, SSD1306_WHITE);
        if (o.big) g.drawRect(x - 9, y - 7, 18, 14, SSD1306_WHITE);
      }
    }
  }

  // снаряд: от носа в точку схода, уменьшаясь
  if (shot.active) {
    float s = (FLOOR_Y - shot.y) / (FLOOR_Y + 1.0f);      // 0 у нас .. 1 вдали
    if (s < 0) s = 0;
    if (s > 1) s = 1;
    int x = (int)(tLaneX(laneX, 1.0f) + (tLaneX((float)shot.col, 0) - tLaneX(laneX, 1.0f)) * s);
    int y = (int)(54 + (VY - 54) * s);
    int r = s < 0.5f ? 2 : 1;
    g.fillRect(x - r / 2, y - r / 2, r, r, SSD1306_WHITE);
  }

  // нос корабля и прицел на текущей полосе
  bool hurtBlink = (c.now < hurtUntil) && ((c.now / 90) & 1);
  int sx = tLaneX(laneX, 1.0f);
  if (!hurtBlink) g.drawBitmap(sx - 8, 56, SPR_SHIP, 16, 8, SSD1306_WHITE);
  int cx = tLaneX(laneX, 0.6f), cy = tDepthY(0.6f) - 6;
  g.drawFastHLine(cx - 3, cy, 7, SSD1306_WHITE);
  g.drawFastVLine(cx, cy - 3, 7, SSD1306_WHITE);

  d.flush();
}

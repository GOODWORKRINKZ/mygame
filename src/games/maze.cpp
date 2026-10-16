#include "games.h"
#include "sprites.h"
#include <math.h>

// ============================================================
//  MAZE — лабиринт от первого лица (1 игрок)
// ============================================================
//  Случайный лабиринт 8x8 клеток (сетка блоков 17x17: стены на чётных
//  координатах). На OLED — вид от первого лица рейкастингом: 64 луча,
//  высота стены по расстоянию, стены вдоль X сплошные, вдоль Y в
//  шахматную штриховку, кристаллы — ромбы по центру клетки, тем
//  крупнее, чем ближе. На матрице — окно карты 4x4 вокруг игрока сверху.
//  ЗЕЛЁНАЯ (тап) — поворот направо, СИНЯЯ (тап) — налево, удержание
//  любой — шаг вперёд. Собери все кристаллы до конца времени.

static const char* KEY_BEST = "maze";
static const int   N = MATRIX_SIZE;
static const int   MW = MAZE_CELLS * 2 + 1;       // 17
static const float FOV = 1.0472f;                 // 60 градусов
static const int   RAYS = 64;                     // по 2 пикселя ширины
static const int   SCENE_Y0 = 18, SCENE_H = 46;   // синяя зона

static inline int mcol(int x) { return MATRIX_COL0 + x; }
static const int DX[4] = { 1, 0, -1, 0 };
static const int DY[4] = { 0, 1, 0, -1 };

void MazeGame::generate() {
  for (int y = 0; y < MW; y++) for (int x = 0; x < MW; x++) wall[y][x] = 1;
  // рекурсивный обход со стеком клеток
  uint8_t stackX[MAZE_CELLS * MAZE_CELLS], stackY[MAZE_CELLS * MAZE_CELLS];
  bool seen[MAZE_CELLS][MAZE_CELLS] = {};
  int sp = 0;
  int cx = 0, cy = 0;
  seen[cy][cx] = true;
  wall[1][1] = 0;
  stackX[sp] = cx; stackY[sp] = cy; sp++;
  while (sp > 0) {
    cx = stackX[sp - 1]; cy = stackY[sp - 1];
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) {
      int nx = cx + DX[d], ny = cy + DY[d];
      if (nx < 0 || ny < 0 || nx >= MAZE_CELLS || ny >= MAZE_CELLS || seen[ny][nx]) continue;
      opts[n++] = d;
    }
    if (n == 0) { sp--; continue; }
    int d = opts[random(0, n)];
    int nx = cx + DX[d], ny = cy + DY[d];
    seen[ny][nx] = true;
    wall[2 * cy + 1 + DY[d]][2 * cx + 1 + DX[d]] = 0;   // стена между
    wall[2 * ny + 1][2 * nx + 1] = 0;                   // сама клетка
    stackX[sp] = nx; stackY[sp] = ny; sp++;
  }
}

void MazeGame::reset() {
  generate();
  cellX = 1; cellY = 1; dir = 0;
  px = 1.5f; py = 1.5f;
  ang = 0; angFrom = 0; angTo = 0;
  moveFrom[0] = moveTo[0] = px; moveFrom[1] = moveTo[1] = py;
  animUntil = 0; animStart = 0; moving = false; turning = false;
  // кристаллы — в случайных клетках, не в стартовой
  for (int i = 0; i < MAZE_CRYSTALS; i++) {
    for (int tries = 0; tries < 50; tries++) {
      int gx = 2 * random(0, MAZE_CELLS) + 1, gy = 2 * random(0, MAZE_CELLS) + 1;
      if (gx == 1 && gy == 1) continue;
      bool dup = false;
      for (int k = 0; k < i; k++) if (crX[k] == gx && crY[k] == gy) dup = true;
      if (dup) continue;
      crX[i] = (uint8_t)gx; crY[i] = (uint8_t)gy; crGot[i] = false;
      break;
    }
  }
  got = 0;
  endAt = millis() + MAZE_TIME_MS;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  won = false;
  overUntil = 0;
  lastOled = 0;
  stepsHeld = 0;
}

bool MazeGame::canStep() const {
  int nx = cellX + 2 * DX[dir], ny = cellY + 2 * DY[dir];
  int wx = cellX + DX[dir], wy = cellY + DY[dir];
  if (nx < 0 || ny < 0 || nx >= MW || ny >= MW) return false;
  return wall[wy][wx] == 0;
}

void MazeGame::update(const Inputs& in, Ctx& c) {
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 8000) { reset(); return; }
    Bg::confetti(c.leds, c.now, won ? rgb(0, 90, 120) : rgb(90, 0, 0), rgb(10, 10, 20));
    char sub[22];
    snprintf(sub, sizeof sub, won ? "ОЧКИ %d" : "КРИСТАЛЛОВ %d", won ? score : got);
    Ui::banner(c.dpy, won ? "ВЫБРАЛСЯ!" : "ВРЕМЯ ВЫШЛО", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- анимации ----
  bool idle = true;
  if (moving || turning) {
    float k = (float)(c.now - animStart) / (float)(animUntil - animStart);
    if (k >= 1.0f) {
      k = 1.0f;
      if (moving) {
        px = moveTo[0]; py = moveTo[1];
        cellX = (int)px; cellY = (int)py;
        // кристалл в клетке?
        for (int i = 0; i < MAZE_CRYSTALS; i++) {
          if (crGot[i] || crX[i] != cellX || crY[i] != cellY) continue;
          crGot[i] = true; got++;
          c.out.sfx(Sfx::Pickup);
          c.out.buzz(25);
          c.fx.flash(rgb(0, 120, 160), 160);
          if (got >= MAZE_CRYSTALS) {
            won = true; over = true; overUntil = c.now + 1200;
            uint32_t left = (endAt > c.now) ? (endAt - c.now) / 1000 : 0;
            score = 100 + (int)left;
            newRecord = store.submit(KEY_BEST, score);
            best = store.best(KEY_BEST);
            c.out.sfx(Sfx::Fanfare);
            return;
          }
        }
      }
      if (turning) ang = angTo;
      moving = turning = false;
    } else {
      if (moving) { px = moveFrom[0] + (moveTo[0] - moveFrom[0]) * k; py = moveFrom[1] + (moveTo[1] - moveFrom[1]) * k; }
      if (turning) ang = angFrom + (angTo - angFrom) * k;
      idle = false;
    }
  }

  // ---- ввод: тап = поворот, удержание = шаг ----
  if (idle) {
    int turn = 0;
    if (in.p1.released && in.p1.lastHeldMs < MAZE_HOLD_MS) turn = +1;
    if (in.p2.released && in.p2.lastHeldMs < MAZE_HOLD_MS) turn = -1;
    bool hold = (in.p1.held && in.p1.heldMs >= MAZE_HOLD_MS) || (in.p2.held && in.p2.heldMs >= MAZE_HOLD_MS);
    if (turn) {
      dir = (dir + turn + 4) % 4;
      angFrom = ang;
      angTo = ang + turn * 1.5708f;
      turning = true;
      animStart = c.now; animUntil = c.now + 150;
      c.out.sfx(Sfx::Click);
    } else if (hold) {
      if (canStep()) {
        moveFrom[0] = px; moveFrom[1] = py;
        moveTo[0] = (float)(cellX + 2 * DX[dir]) + 0.5f;
        moveTo[1] = (float)(cellY + 2 * DY[dir]) + 0.5f;
        moving = true;
        animStart = c.now; animUntil = c.now + 180;
        c.out.sfx(Sfx::Tick);
      } else if (stepsHeld == 0) {
        c.out.sfx(Sfx::Bounce);           // упёрся в стену
      }
      stepsHeld++;
    } else {
      stepsHeld = 0;
    }
  }

  // ---- время ----
  if (c.now >= endAt) {
    over = true; won = false; overUntil = c.now + 1200;
    score = got * 10;
    newRecord = store.submit(KEY_BEST, score);
    best = store.best(KEY_BEST);
    c.out.sfx(Sfx::GameOver);
    return;
  }

  render(c);
  if (c.now - lastOled >= 50) { lastOled = c.now; oled(c); }
}

// ------------------------------------------------------------
//  Лента: окно карты 4x4 вокруг игрока
// ------------------------------------------------------------
void MazeGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();
  int wx0 = cellX - 1, wy0 = cellY - 2;
  if (wx0 < 0) wx0 = 0; if (wy0 < 0) wy0 = 0;
  if (wx0 > MW - N) wx0 = MW - N; if (wy0 > MW - N) wy0 = MW - N;
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      int gx = wx0 + x, gy = wy0 + y;
      Color col = wall[gy][gx] ? rgb(70, 28, 0) : rgb(2, 2, 5);
      for (int i = 0; i < MAZE_CRYSTALS; i++)
        if (!crGot[i] && crX[i] == gx && crY[i] == gy) col = rgb(0, 200, 255);
      if (gx == cellX + DX[dir] && gy == cellY + DY[dir] && !wall[gy][gx]) col = rgb(50, 50, 60);
      if (gx == cellX && gy == cellY) col = rgb(255, 255, 255);
      L.setXY(mcol(x), y, col);
    }
  // рукав зелёного — время, синего — кристаллы
  uint32_t left = (endAt > c.now) ? endAt - c.now : 0;
  float k = (float)left / (float)MAZE_TIME_MS;
  Color tc = colLerp(rgb(255, 30, 0), rgb(0, 220, 80), (uint8_t)(k * 255));
  float lit = k * ARM_P1.len;
  for (int i = 0; i < ARM_P1.len; i++) {
    float f = lit - i;
    uint8_t b = f >= 1.0f ? 140 : (f > 0 ? (uint8_t)(f * 140) : 0);
    L.set(ARM_P1, i, colScale(tc, (uint8_t)(6 + b)));
  }
  for (int i = 0; i < ARM_P2.len; i++)
    L.set(ARM_P2, i, (i < got) ? rgb(0, 200, 255) : (i < MAZE_CRYSTALS ? rgb(0, 10, 14) : 0));
}

// ------------------------------------------------------------
//  OLED: рейкастинг
// ------------------------------------------------------------
void MazeGame::oled(Ctx& c) {
  Display& d = c.dpy;
  Adafruit_SSD1306& g = d.gfx();
  d.clear();

  char top[26];
  uint32_t left = (endAt > c.now) ? (endAt - c.now) / 1000 : 0;
  snprintf(top, sizeof top, "ЛАБИРИНТ  %d/%d", got, MAZE_CRYSTALS);
  d.text(0, 1, top, 1);
  char tm[10];
  snprintf(tm, sizeof tm, "%lu С", (unsigned long)left);
  d.text(OLED_WIDTH - Display::textWidth(tm, 1), 1, tm, 1);
  g.drawFastHLine(0, 15, OLED_WIDTH, SSD1306_WHITE);

  float zbuf[RAYS];
  const int mid = SCENE_Y0 + SCENE_H / 2;

  for (int i = 0; i < RAYS; i++) {
    float ra = ang - FOV / 2 + FOV * ((float)i + 0.5f) / RAYS;
    float rdx = cosf(ra), rdy = sinf(ra);
    int mx = (int)px, my = (int)py;
    float ddx = (rdx == 0) ? 1e9f : fabsf(1.0f / rdx);
    float ddy = (rdy == 0) ? 1e9f : fabsf(1.0f / rdy);
    int sx, sy; float sdx, sdy;
    if (rdx < 0) { sx = -1; sdx = (px - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - px) * ddx; }
    if (rdy < 0) { sy = -1; sdy = (py - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - py) * ddy; }
    int side = 0;
    for (int it = 0; it < 64; it++) {
      if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
      if (mx < 0 || my < 0 || mx >= MW || my >= MW || wall[my][mx]) break;
    }
    float dist = (side == 0) ? (sdx - ddx) : (sdy - ddy);
    if (dist < 0.05f) dist = 0.05f;
    // перпендикулярное расстояние уже получено из DDA (без рыбьего глаза)
    zbuf[i] = dist;
    int h = (int)(SCENE_H / dist);
    if (h > SCENE_H) h = SCENE_H;
    int y0 = mid - h / 2, y1 = mid + h / 2;
    if (y0 < SCENE_Y0) y0 = SCENE_Y0;
    if (y1 > SCENE_Y0 + SCENE_H - 1) y1 = SCENE_Y0 + SCENE_H - 1;
    int x = i * 2;
    if (side == 0) {
      g.drawFastVLine(x, y0, y1 - y0 + 1, SSD1306_WHITE);
      g.drawFastVLine(x + 1, y0, y1 - y0 + 1, SSD1306_WHITE);
    } else {
      // шахматная штриховка
      for (int y = y0; y <= y1; y++) g.drawPixel(x + (y & 1), y, SSD1306_WHITE);
    }
  }

  // кристаллы — ромбы, с проверкой по буферу глубины
  for (int i = 0; i < MAZE_CRYSTALS; i++) {
    if (crGot[i]) continue;
    float dx = (float)crX[i] + 0.5f - px, dy = (float)crY[i] + 0.5f - py;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < 0.3f) continue;
    float rel = atan2f(dy, dx) - ang;
    while (rel > 3.14159f) rel -= 6.28318f;
    while (rel < -3.14159f) rel += 6.28318f;
    if (fabsf(rel) > FOV / 2 + 0.3f) continue;
    float perp = dist * cosf(rel);
    int sx = (int)(64 + (rel / (FOV / 2)) * 64);
    int col = sx / 2;
    if (col < 0 || col >= RAYS || perp > zbuf[col]) continue;
    int s = (int)(SCENE_H / perp * 0.22f);
    if (s < 1) s = 1;
    if (s > 10) s = 10;
    int cy = mid + (int)(SCENE_H / perp * 0.12f);       // лежит на полу
    bool blink = ((c.now / 120) & 1);
    for (int k = -s; k <= s; k++) {
      int w = s - abs(k);
      if (blink || (k & 1) == 0) g.drawFastHLine(sx - w, cy + k, 2 * w + 1, SSD1306_WHITE);
    }
  }

  d.flush();
}

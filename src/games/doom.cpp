#include "games.h"
#include "sprites.h"
#include <math.h>

// ============================================================
//  DOOM — арена от первого лица (1 игрок)
// ============================================================
//  Две кнопки на всё:
//    ничего не нажато — идёшь вперёд сам (скользишь вдоль стен);
//    держишь ЗЕЛЁНУЮ — стоишь и поворачиваешься вправо, СИНЮЮ — влево;
//    зажал ОБЕ — стоишь и стреляешь очередями.
//  Арена 16x16 с колоннами, демоны появляются волнами у краёв и идут
//  к тебе; подошёл вплотную — кусает. Два попадания — демон лопается.
//  Волна зачищена — следующая больше, и немного здоровья в награду.
//  OLED: рейкастинг стен + спрайты демонов с масштабом по дальности и
//  проверкой по z-буферу, дробовик снизу с отдачей и вспышкой.
//  Матрица: радар 4x4 (клетка = 2x2 арены), ты в центре, демоны красным.

static const char* KEY_BEST = "doom";
static const int   N = MATRIX_SIZE;
static const int   MW = DOOM_MAP;                // 16
static const float FOV = 1.0472f;
static const int   RAYS = 64;
static const int   SCENE_Y0 = 18, SCENE_H = 46;
static const int   MID = SCENE_Y0 + SCENE_H / 2;

static inline int mcol(int x) { return MATRIX_COL0 + x; }

// '#' — стена, '.' — пол. Старт в центре.
static const char* ARENA[MW] = {
  "################",
  "#..............#",
  "#..##......##..#",
  "#..##......##..#",
  "#......##......#",
  "#......##......#",
  "#..#........#..#",
  "#..#........#..#",
  "#..#........#..#",
  "#..#........#..#",
  "#......##......#",
  "#......##......#",
  "#..##......##..#",
  "#..##......##..#",
  "#..............#",
  "################",
};
static inline bool isWall(int x, int y) {
  if (x < 0 || y < 0 || x >= MW || y >= MW) return true;
  return ARENA[y][x] == '#';
}

void DoomGame::reset() {
  px = 8.0f; py = 8.0f; ang = -1.5708f;
  angFrom = angTo = ang; turnStart = turnUntil = 0;
  firedDuring[0] = firedDuring[1] = false;
  hp = DOOM_HP;
  wave = 0;
  kills = 0;
  alive = 0;
  toSpawn = 0;
  spawnIn = 0;
  for (auto& e : foes) e.active = false;
  fireCd = 0;
  muzzleUntil = 0;
  hurtUntil = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  lastOled = 0;
  // углы клеток кольца вокруг центра матрицы (столбец 9.5, ряд 1.5),
  // ряд растёт вниз — как экранные координаты
  for (int i = 0; i < RING.len; i++) {
    int ph = RING.idx[i];
    float cx = 0, cy = 0;
    for (int r = 0; r < LED_GRID_ROWS; r++)
      for (int cc = 0; cc < LED_GRID_COLS; cc++)
        if (LED_GRID[r][cc] == ph) { cx = (float)cc; cy = (float)r; }
    ringAng[i] = atan2f(cy - 1.5f, cx - (MATRIX_COL0 + 1.5f));
  }
  nextWave(millis());
}

void DoomGame::nextWave(uint32_t now) {
  wave++;
  toSpawn = 1 + wave;
  spawnIn = 1.2f;
  hp += DOOM_WAVE_HEAL;
  if (hp > DOOM_HP) hp = DOOM_HP;
  waveBannerUntil = now + 1500;
}

void DoomGame::spawnFoe() {
  // у краёв арены, не ближе 5 клеток к игроку
  for (int tries = 0; tries < 30; tries++) {
    int x = random(1, MW - 1), y = random(1, MW - 1);
    if (x > 2 && x < MW - 3 && y > 2 && y < MW - 3) continue;
    if (isWall(x, y)) continue;
    float dx = x + 0.5f - px, dy = y + 0.5f - py;
    if (dx * dx + dy * dy < 25.0f) continue;
    for (auto& e : foes) {
      if (e.active) continue;
      e.active = true; e.dying = false;
      e.x = x + 0.5f; e.y = y + 0.5f;
      e.hp = DOOM_FOE_HP;
      e.biteAt = 0;
      e.hitUntil = 0;
      e.speed = DOOM_FOE_SPEED + wave * 0.06f;
      e.growled = false;
      alive++;
      return;
    }
    return;
  }
}

// Шаг с проверкой столкновений по осям — скользим вдоль стен.
void DoomGame::tryMove(float dx, float dy) {
  const float R = 0.25f;
  float nx = px + dx;
  if (!isWall((int)(nx + (dx > 0 ? R : -R)), (int)(py - R)) &&
      !isWall((int)(nx + (dx > 0 ? R : -R)), (int)(py + R))) px = nx;
  float ny = py + dy;
  if (!isWall((int)(px - R), (int)(ny + (dy > 0 ? R : -R))) &&
      !isWall((int)(px + R), (int)(ny + (dy > 0 ? R : -R)))) py = ny;
}

// Дистанция до стены вдоль луча (для проверки, что демон не за стеной).
float DoomGame::wallDist(float a) const {
  float rdx = cosf(a), rdy = sinf(a);
  int mx = (int)px, my = (int)py;
  float ddx = (rdx == 0) ? 1e9f : fabsf(1.0f / rdx);
  float ddy = (rdy == 0) ? 1e9f : fabsf(1.0f / rdy);
  int sx, sy; float sdx, sdy;
  if (rdx < 0) { sx = -1; sdx = (px - mx) * ddx; } else { sx = 1; sdx = (mx + 1.0f - px) * ddx; }
  if (rdy < 0) { sy = -1; sdy = (py - my) * ddy; } else { sy = 1; sdy = (my + 1.0f - py) * ddy; }
  int side = 0;
  for (int it = 0; it < 40; it++) {
    if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
    if (isWall(mx, my)) break;
  }
  return (side == 0) ? (sdx - ddx) : (sdy - ddy);
}

void DoomGame::fire(Ctx& c) {
  fireCd = c.now + DOOM_FIRE_MS;
  muzzleUntil = c.now + 90;
  c.out.sfx(Sfx::Shoot);
  c.out.buzz(25);

  // ближайший демон в створе прицела и не за стеной
  Foe* target = nullptr;
  float bestD = 99.0f;
  for (auto& e : foes) {
    if (!e.active || e.dying) continue;
    float dx = e.x - px, dy = e.y - py;
    float dist = sqrtf(dx * dx + dy * dy);
    float rel = atan2f(dy, dx) - ang;
    while (rel > 3.14159f) rel -= 6.28318f;
    while (rel < -3.14159f) rel += 6.28318f;
    float tol = atan2f(0.45f, dist);              // половина ширины демона
    if (tol < DOOM_AIM_HALF) tol = DOOM_AIM_HALF; // при шаге поворота 45° створ щедрый
    if (fabsf(rel) > tol) continue;
    if (dist > wallDist(ang)) continue;
    if (dist < bestD) { bestD = dist; target = &e; }
  }
  if (!target) return;
  target->hp--;
  target->hitUntil = c.now + 120;
  if (target->hp <= 0) {
    target->dying = true;
    target->dieUntil = c.now + 300;
    alive--;
    kills++;
    c.out.sfx(Sfx::Explode);
    c.out.buzzPattern(2, 50, 40);
  } else {
    c.out.sfx(Sfx::Hit);
  }
}

void DoomGame::update(const Inputs& in, Ctx& c) {
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 8000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(140, 0, 0), rgb(30, 0, 0));
    char sub[22];
    snprintf(sub, sizeof sub, "ВОЛНА %d  УБИТО %d", wave, kills);
    Ui::banner(c.dpy, "ТЫ ПОГИБ", sub, newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- управление: тап = поворот на 45°, держишь одну = идёшь,
  //      держишь обе = стоишь и стреляешь ----
  if (in.p1.pressed) firedDuring[0] = false;
  if (in.p2.pressed) firedDuring[1] = false;
  bool g = in.p1.held, b = in.p2.held;

  if (turnUntil) {                                   // анимация поворота
    if (c.now >= turnUntil) { ang = angTo; turnUntil = 0; }
    else {
      float k = (float)(c.now - turnStart) / (float)(turnUntil - turnStart);
      ang = angFrom + (angTo - angFrom) * k;
    }
  }

  if (g && b) {
    if (c.now >= fireCd) { fire(c); firedDuring[0] = firedDuring[1] = true; }
  } else {
    // тап — поворот (если за это нажатие не стреляли и вторая не зажата)
    int turn = 0;
    if (in.p1.released && in.p1.lastHeldMs < DOOM_TAP_MS && !firedDuring[0] && !b) turn = +1;
    if (in.p2.released && in.p2.lastHeldMs < DOOM_TAP_MS && !firedDuring[1] && !g) turn = -1;
    if (turn && !turnUntil) {
      angFrom = ang;
      angTo = ang + turn * DOOM_TURN_STEP;
      turnStart = c.now; turnUntil = c.now + DOOM_TURN_MS;
      c.out.tone(500, 10);
    }
    // удержание одной — ход вперёд
    bool walk = (g && in.p1.heldMs >= DOOM_TAP_MS) || (b && in.p2.heldMs >= DOOM_TAP_MS);
    if (walk && !turnUntil && c.now >= waveBannerUntil)
      tryMove(cosf(ang) * DOOM_WALK_SPEED * c.dt, sinf(ang) * DOOM_WALK_SPEED * c.dt);
  }
  while (ang > 3.14159f) { ang -= 6.28318f; angFrom -= 6.28318f; angTo -= 6.28318f; }
  while (ang < -3.14159f) { ang += 6.28318f; angFrom += 6.28318f; angTo += 6.28318f; }

  // ---- спавн волны ----
  if (toSpawn > 0) {
    spawnIn -= c.dt;
    if (spawnIn <= 0) { spawnFoe(); toSpawn--; spawnIn = 1.6f - wave * 0.1f; if (spawnIn < 0.5f) spawnIn = 0.5f; }
  } else if (alive <= 0) {
    nextWave(c.now);
    c.out.sfx(Sfx::LevelUp);
    c.fx.flash(rgb(0, 120, 60), 250);
  }

  // ---- демоны ----
  for (auto& e : foes) {
    if (!e.active) continue;
    if (e.dying) { if (c.now >= e.dieUntil) e.active = false; continue; }
    float dx = px - e.x, dy = py - e.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (!e.growled && dist < 2.5f) { e.growled = true; c.out.tone(110, 90); c.out.buzz(15); }
    if (dist > 0.9f) {
      float step = e.speed * c.dt;
      float nx = e.x + dx / dist * step, ny = e.y + dy / dist * step;
      if (!isWall((int)nx, (int)e.y)) e.x = nx;
      if (!isWall((int)e.x, (int)ny)) e.y = ny;
    } else if (c.now >= e.biteAt) {
      e.biteAt = c.now + DOOM_BITE_MS;
      hp -= DOOM_BITE_DMG;
      hurtUntil = c.now + 250;
      c.out.sfx(Sfx::Error);
      c.out.buzzPattern(2, 80, 60);
      c.fx.flash(rgb(180, 0, 0), 200);
      if (hp <= 0) {
        hp = 0;
        over = true; overUntil = c.now + 1200;
        newRecord = store.submit(KEY_BEST, kills);
        best = store.best(KEY_BEST);
        c.out.sfx(Sfx::GameOver);
        return;
      }
    }
  }

  render(c);
  if (c.now - lastOled >= 50) { lastOled = c.now; oled(c); }
}

// ------------------------------------------------------------
//  Лента: радар
// ------------------------------------------------------------
void DoomGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- кольцо — радар относительно взгляда: верх матрицы = прямо
  //      перед тобой, яркость = близость, мигание = кусает ----
  for (int i = 0; i < RING.len; i++) L.set(RING, i, rgb(4, 4, 6));
  for (auto& e : foes) {
    if (!e.active || e.dying) continue;
    float dx = e.x - px, dy = e.y - py;
    float dist = sqrtf(dx * dx + dy * dy);
    float rel = atan2f(dy, dx) - ang;                 // 0 = прямо, >0 = справа
    float scr = -1.5708f + rel;                       // экранный угол: вверх = прямо
    int bestI = 0; float bestD = 99;
    for (int i = 0; i < RING.len; i++) {
      float d = scr - ringAng[i];
      while (d > 3.14159f) d -= 6.28318f;
      while (d < -3.14159f) d += 6.28318f;
      if (fabsf(d) < bestD) { bestD = fabsf(d); bestI = i; }
    }
    float k = 1.0f - (dist - 1.0f) / 9.0f;
    if (k < 0.15f) k = 0.15f;
    if (k > 1.0f) k = 1.0f;
    uint8_t bright = (uint8_t)(40 + 215 * k);
    if (dist < 1.3f && ((c.now / 90) & 1)) bright = 20;
    L.add(RING, bestI, colScale(rgb(255, 0, 0), bright));
  }

  // ---- сердцевина — здоровье ----
  float k = (float)hp / (float)DOOM_HP;
  Color hc = colLerp(rgb(255, 20, 0), rgb(0, 220, 60), (uint8_t)(k * 255));
  uint8_t hb = (c.now < hurtUntil) ? (((c.now / 60) & 1) ? 255 : 60) : (uint8_t)(60 + 120 * k);
  L.fill(CORE, colScale(hc, hb));

  // ---- рукав зелёного — здоровье шкалой, синего — демоны волны ----
  float lit = k * ARM_P1.len;
  for (int i = 0; i < ARM_P1.len; i++) {
    float f = lit - i;
    uint8_t bb = f >= 1.0f ? 150 : (f > 0 ? (uint8_t)(f * 150) : 0);
    L.set(ARM_P1, i, colScale(hc, (uint8_t)(6 + bb)));
  }
  int total = 1 + wave, left = toSpawn + alive;
  for (int i = 0; i < ARM_P2.len; i++) {
    bool on = i < total && i < ARM_P2.len;
    bool dead = on && i >= left;
    L.set(ARM_P2, i, !on ? 0 : (dead ? rgb(10, 4, 4) : rgb(160, 0, 0)));
  }
}

// ------------------------------------------------------------
//  OLED: рейкастинг + спрайты
// ------------------------------------------------------------
static void drawScaled(Adafruit_SSD1306& g, const uint8_t* bmp, int bw, int bh,
                       int cx, int bottom, int size, const float* zbuf, float dist) {
  if (size < 2) { g.drawPixel(cx, bottom - 1, SSD1306_WHITE); return; }
  int x0 = cx - size / 2, y0 = bottom - size;
  int bytesPerRow = (bw + 7) / 8;
  // чёрная подложка: вырез в стене, иначе белый демон на белой стене невидим
  for (int sy = -1; sy <= size; sy++) {
    int yy = y0 + sy;
    if (yy < SCENE_Y0 || yy > SCENE_Y0 + SCENE_H - 1) continue;
    for (int sx = -1; sx <= size; sx++) {
      int xx = x0 + sx;
      if (xx < 0 || xx >= OLED_WIDTH) continue;
      if (zbuf[xx / 2] < dist) continue;
      g.drawPixel(xx, yy, SSD1306_BLACK);
    }
  }
  for (int sy = 0; sy < size; sy++) {
    int yy = y0 + sy;
    if (yy < SCENE_Y0 || yy > SCENE_Y0 + SCENE_H - 1) continue;
    int srcY = sy * bh / size;
    for (int sx = 0; sx < size; sx++) {
      int xx = x0 + sx;
      if (xx < 0 || xx >= OLED_WIDTH) continue;
      if (zbuf[xx / 2] < dist) continue;
      int srcX = sx * bw / size;
      uint8_t byte = pgm_read_byte(bmp + srcY * bytesPerRow + srcX / 8);
      if (byte & (0x80 >> (srcX & 7))) g.drawPixel(xx, yy, SSD1306_WHITE);
    }
  }
}

void DoomGame::oled(Ctx& c) {
  Display& d = c.dpy;
  Adafruit_SSD1306& g = d.gfx();
  d.clear();

  char top[26];
  snprintf(top, sizeof top, "ЗД %d  ВОЛНА %d", hp, wave);
  d.text(0, 1, top, 1);
  char r[12];
  snprintf(r, sizeof r, "УБИТО %d", kills);
  d.text(OLED_WIDTH - Display::textWidth(r, 1), 1, r, 1);
  g.drawFastHLine(0, 15, OLED_WIDTH, SSD1306_WHITE);

  // ---- стены ----
  float zbuf[RAYS];
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
    for (int it = 0; it < 40; it++) {
      if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
      if (isWall(mx, my)) break;
    }
    float dist = (side == 0) ? (sdx - ddx) : (sdy - ddy);
    if (dist < 0.05f) dist = 0.05f;
    zbuf[i] = dist;
    int h = (int)(SCENE_H / dist);
    if (h > SCENE_H) h = SCENE_H;
    int y0 = MID - h / 2, y1 = MID + h / 2;
    if (y0 < SCENE_Y0) y0 = SCENE_Y0;
    if (y1 > SCENE_Y0 + SCENE_H - 1) y1 = SCENE_Y0 + SCENE_H - 1;
    int x = i * 2;
    if (side == 0) {
      g.drawFastVLine(x, y0, y1 - y0 + 1, SSD1306_WHITE);
      g.drawFastVLine(x + 1, y0, y1 - y0 + 1, SSD1306_WHITE);
    } else {
      for (int y = y0; y <= y1; y++) g.drawPixel(x + (y & 1), y, SSD1306_WHITE);
    }
  }

  // ---- демоны, дальние первыми ----
  for (int pass = 0; pass < 2; pass++) {
    for (auto& e : foes) {
      if (!e.active) continue;
      float dx = e.x - px, dy = e.y - py;
      float dist = sqrtf(dx * dx + dy * dy);
      bool far = dist > 3.0f;
      if ((pass == 0) != far) continue;
      if (dist < 0.3f) continue;
      float rel = atan2f(dy, dx) - ang;
      while (rel > 3.14159f) rel -= 6.28318f;
      while (rel < -3.14159f) rel += 6.28318f;
      if (fabsf(rel) > FOV / 2 + 0.4f) continue;
      float perp = dist * cosf(rel);
      if (perp < 0.2f) perp = 0.2f;
      int sx = (int)(64 + (rel / (FOV / 2)) * 64);
      int h = (int)(SCENE_H / perp);
      int size = (int)(h * 0.85f);
      if (size > 60) size = 60;
      int bottom = MID + h / 2;
      if (e.dying) {
        // лопается: расходящиеся кольца
        float k = 1.0f - (float)(e.dieUntil - c.now) / 300.0f;
        int rr = (int)(size / 2 * (0.3f + k));
        if (rr > 0) g.drawCircle(sx, bottom - size / 2, rr, SSD1306_WHITE);
        if (rr > 3) g.drawCircle(sx, bottom - size / 2, rr / 2, SSD1306_WHITE);
        continue;
      }
      bool flash = (c.now < e.hitUntil) && ((c.now / 40) & 1);
      if (flash) continue;
      drawScaled(g, SPR_DEMON, 16, 16, sx, bottom, size, zbuf, perp);
    }
  }

  // ---- стрелки у краёв: демон вне поля зрения ----
  for (auto& e : foes) {
    if (!e.active || e.dying) continue;
    float dx = e.x - px, dy = e.y - py;
    float dist = sqrtf(dx * dx + dy * dy);
    float rel = atan2f(dy, dx) - ang;
    while (rel > 3.14159f) rel -= 6.28318f;
    while (rel < -3.14159f) rel += 6.28318f;
    if (fabsf(rel) <= FOV / 2) continue;
    int sz = dist < 2.0f ? 6 : (dist < 5.0f ? 4 : 3);
    bool blink = dist < 1.5f && ((c.now / 100) & 1);
    if (blink) continue;
    if (rel > 0) g.fillTriangle(127, MID, 127 - sz, MID - sz, 127 - sz, MID + sz, SSD1306_WHITE);
    else         g.fillTriangle(0, MID, sz, MID - sz, sz, MID + sz, SSD1306_WHITE);
    if (fabsf(rel) > 2.4f) {   // почти за спиной — стрелка и снизу
      int bx = rel > 0 ? 110 : 18;
      g.fillTriangle(bx, 62, bx - 3, 58, bx + 3, 58, SSD1306_WHITE);
    }
  }

  // ---- прицел ----
  g.drawFastHLine(62, MID, 5, SSD1306_WHITE);
  g.drawFastVLine(64, MID - 2, 5, SSD1306_WHITE);

  // ---- дробовик с отдачей и вспышкой ----
  int kick = (c.now < muzzleUntil) ? 3 : 0;
  int gx = 70, gy = 64 - 16 + kick;
  g.drawBitmap(gx, gy, SPR_SHOTGUN, 24, 16, SSD1306_WHITE);
  if (c.now < muzzleUntil) {
    int fx = gx + 11, fy = gy - 2;
    g.drawLine(fx - 5, fy - 4, fx + 5, fy + 2, SSD1306_WHITE);
    g.drawLine(fx + 5, fy - 4, fx - 5, fy + 2, SSD1306_WHITE);
    g.drawFastVLine(fx, fy - 6, 8, SSD1306_WHITE);
    g.drawFastHLine(fx - 6, fy - 1, 12, SSD1306_WHITE);
  }

  // ---- укус: рамка по краю сцены ----
  if (c.now < hurtUntil) g.drawRect(0, SCENE_Y0, OLED_WIDTH, SCENE_H, SSD1306_WHITE);

  // ---- баннер волны ----
  if (c.now < waveBannerUntil) {
    char w[12];
    snprintf(w, sizeof w, "ВОЛНА %d", wave);
    int tw = Display::textWidth(w, 2);
    g.fillRect(64 - tw / 2 - 3, MID - 9, tw + 6, 18, SSD1306_BLACK);
    g.drawRect(64 - tw / 2 - 3, MID - 9, tw + 6, 18, SSD1306_WHITE);
    d.textCentered(MID - 7, w, 2);
  }

  d.flush();
}

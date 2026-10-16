#include "games.h"
#include "sprites.h"
#include <math.h>

// ============================================================
//  DINO — динозаврик в пустыне (1 игрок, матрица 4x4 + OLED в 3D)
// ============================================================
//  На ленте — вид сверху: динозаврик стоит точкой в нижнем ряду
//  матрицы, сверху по четырём колонкам набегают камни и кактусы.
//  ЗЕЛЁНАЯ — шаг вправо, СИНЯЯ — влево. Задел препятствие — минус
//  жизнь. Чем дольше бежишь, тем быстрее и плотнее.
//
//  На OLED в это время — вид сзади, "из-за плеча": дорога сходится к
//  горизонту, препятствия растут по мере приближения и уезжают вниз
//  экрана, динозаврик бежит и перебегает между четырьмя полосами.
//  Жёлтая полоса экрана — табло, синяя — сцена.

static const char* KEY_BEST = "dino";
static const int   N = MATRIX_SIZE;
static const float FLOOR_Y = (float)(N - 1);

static inline int mcol(int x) { return MATRIX_COL0 + x; }

void DinoGame::reset() {
  for (auto& o : obs) o.active = false;
  lane = 1;
  laneX = 1.0f;
  speed = DINO_SPEED_START;
  spawnIn = 1.2f;
  lives = DINO_LIVES;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  hurtUntil = 0;
  dist = 0;
  lastLane = -1;
}

// В каких полосах дино может оказаться к моменту нового ряда.
// Идём от дино вверх по уже стоящим рядам: между соседними рядами он
// успевает сдвинуться не больше чем на одну полосу, а полосы с
// препятствиями из набора выпадают. Возвращает битовую маску полос.
uint8_t DinoGame::reachableLanes() const {
  // ряды сверху вниз по y: ближайший к дино — последним в проходе снизу
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
  // сортировка по убыванию y (ближние к дино первыми)
  for (int i = 1; i < nr; i++)
    for (int j = i; j > 0 && rows[j].y > rows[j - 1].y; j--) { Row t = rows[j]; rows[j] = rows[j - 1]; rows[j - 1] = t; }

  uint8_t reach = (uint8_t)(1 << lane);
  for (int i = 0; i < nr; i++) {
    uint8_t grown = (uint8_t)((reach | (reach << 1) | (reach >> 1)) & 0x0F);
    reach = (uint8_t)(grown & ~rows[i].mask);
    if (!reach) return 0;                   // уже тупик — пусть хоть новый ряд не добавит
  }
  // к новому ряду — ещё один шаг
  return (uint8_t)((reach | (reach << 1) | (reach >> 1)) & 0x0F);
}

// Новый ряд препятствий: одна или две клетки, никогда все четыре, и
// обязательно так, чтобы у дино осталась достижимая свободная полоса.
void DinoGame::spawnRow() {
  uint8_t reach = reachableLanes();
  if (reach == 0) reach = 0x0F;              // тупик уже есть — не усугубляем логикой, просто не блокируем всё

  int n = (score >= 6 && random(100) < 35) ? 2 : 1;
  int c1 = -1, c2 = -1;
  for (int tries = 0; tries < 12; tries++) {
    c1 = (int)random(0, N);
    if (c1 == lastLane && random(2)) c1 = (c1 + 1) % N;
    c2 = -1;
    if (n == 2) {
      c2 = (int)random(0, N);
      if (c2 == c1) c2 = (c1 + 2) % N;
    }
    uint8_t blocked = (uint8_t)((1 << c1) | (c2 >= 0 ? (1 << c2) : 0));
    if (reach & ~blocked) break;             // остаётся достижимая свободная полоса
    if (tries >= 6) n = 1;                   // не выходит с двумя — ставим одно
    c1 = -1;
  }
  if (c1 < 0) {
    // крайний случай: одна клетка в любой недостижимой полосе, иначе пропускаем ряд
    for (int c = 0; c < N; c++) if (!(reach & (1 << c))) { c1 = c; break; }
    if (c1 < 0) return;
    c2 = -1;
  }
  lastLane = c1;
  for (auto& o : obs) {
    if (o.active) continue;
    if (c1 >= 0) {
      o.active = true; o.col = (uint8_t)c1; o.y = -1.0f; o.kind = (uint8_t)random(2); o.judged = false;
      c1 = -1;
    } else if (c2 >= 0) {
      o.active = true; o.col = (uint8_t)c2; o.y = -1.0f; o.kind = (uint8_t)random(2); o.judged = false;
      c2 = -1;
    }
    if (c1 < 0 && c2 < 0) break;
  }
}

void DinoGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(120, 70, 0), rgb(40, 20, 0));
    char sub[22];
    snprintf(sub, sizeof sub, "ПРОБЕЖАЛ %d", score);
    Ui::banner(c.dpy, "СПОТКНУЛСЯ", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- полосы ----
  if (in.p1.pressed && lane < N - 1) { lane++; c.out.sfx(Sfx::Click); }
  if (in.p2.pressed && lane > 0)     { lane--; c.out.sfx(Sfx::Click); }
  // на экране дино перебегает плавно
  float dxl = (float)lane - laneX;
  float stepL = 9.0f * c.dt;
  if (fabsf(dxl) <= stepL) laneX = (float)lane; else laneX += (dxl > 0 ? stepL : -stepL);

  // ---- пустыня бежит ----
  dist += speed * c.dt;

  // ---- спавн: расстояние между рядами в клетках держим ~постоянным ----
  spawnIn -= speed * c.dt;
  if (spawnIn <= 0) {
    spawnRow();
    float gapRows = 2.0f - score * 0.02f;
    if (gapRows < DINO_GAP_MIN_ROWS) gapRows = DINO_GAP_MIN_ROWS;
    spawnIn = gapRows * ((float)random(85, 120) / 100.0f);
  }

  // ---- препятствия ----
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
          over = true;
          overUntil = c.now + 1200;
          newRecord = store.submit(KEY_BEST, score);
          best = store.best(KEY_BEST);
          c.out.sfx(Sfx::GameOver);
          return;
        }
      } else {
        score++;
        speed += DINO_SPEED_UP;
        if (speed > DINO_SPEED_MAX) speed = DINO_SPEED_MAX;
        if (score % 5 == 0) c.out.sfx(Sfx::Pickup);
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
void DinoGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // песок: редкие тусклые точки бегут вниз вместе с дорогой
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      int k = (int)(dist * 2.0f) + y * 3 + x * 7;
      bool grain = (k % 11) == 0;
      L.setXY(mcol(x), y, grain ? rgb(22, 16, 4) : rgb(5, 4, 1));
    }

  // препятствия: камень оранжевый, кактус зелёный
  for (auto& o : obs) {
    if (!o.active) continue;
    Color col = (o.kind == 0) ? rgb(255, 90, 0) : rgb(40, 255, 60);
    L.addAAXY((float)mcol(o.col), o.y, col);
  }

  // дино: белый, после удара мигает красным
  Color me = rgb(240, 240, 255);
  if (c.now < hurtUntil) me = ((c.now / 70) & 1) ? rgb(255, 0, 0) : rgb(80, 0, 0);
  L.setXY(mcol(lane), N - 1, me);

  // рукав зелёного: жизни; рукав синего: скорость
  for (int i = 0; i < ARM_P1.len; i++)
    L.set(ARM_P1, i, (i < lives) ? colScale(rgb(255, 40, 40), 140) : rgb(6, 2, 2));
  float k = (speed - DINO_SPEED_START) / (DINO_SPEED_MAX - DINO_SPEED_START);
  float lit = k * ARM_P2.len;
  for (int i = 0; i < ARM_P2.len; i++) {
    float f = lit - i;
    uint8_t b = f >= 1.0f ? 120 : (f > 0 ? (uint8_t)(f * 120) : 0);
    L.set(ARM_P2, i, colScale(rgb(255, 200, 0), (uint8_t)(8 + b)));
  }
}

// ------------------------------------------------------------
//  OLED: вид сзади с перспективой
// ------------------------------------------------------------
static const int HORIZON   = 27;     // линия горизонта
static const int GROUND    = 63;     // низ экрана
static const int CX        = 64;

// Экранная X полосы lane (0..3, дробная) на глубине t: 0 = горизонт, 1 = у нас.
static inline int laneScreenX(float lane, float t) {
  // у низа экрана полосы через 30 px: 4 полосы + спрайт 16 px влезают в 128,
  // края дороги (полосы -0.5 и 3.5) — на x=4 и x=124
  float spread = 5.0f + 25.0f * t;
  return (int)(CX + (lane - 1.5f) * spread + 0.5f);
}
static inline int depthScreenY(float t) {
  return (int)(HORIZON + t * t * (GROUND - HORIZON) + 0.5f);   // квадрат: дальнее сжато
}

// Препятствие по глубине: далеко — точка, средне — малый спрайт, близко — большой.
static void drawObstacle(Adafruit_SSD1306& g, uint8_t kind, int x, int yb, float t) {
  if (t < 0.3f) {
    if (kind == 0) g.drawFastHLine(x - 1, yb, 3, SSD1306_WHITE);
    else           g.drawFastVLine(x, yb - 2, 3, SSD1306_WHITE);
  } else if (t < 0.65f) {
    if (kind == 0) g.drawBitmap(x - 4, yb - 4, SPR_ROCK_S, 8, 4, SSD1306_WHITE);
    else           g.drawBitmap(x - 2, yb - 8, SPR_CACTUS_S, 4, 8, SSD1306_WHITE);
  } else {
    if (kind == 0) g.drawBitmap(x - 8, yb - 8, SPR_ROCK_L, 16, 8, SSD1306_WHITE);
    else           g.drawBitmap(x - 4, yb - 16, SPR_CACTUS_L, 8, 16, SSD1306_WHITE);
  }
}

void DinoGame::oled(Ctx& c) {
  Display& d = c.dpy;
  Adafruit_SSD1306& g = d.gfx();
  d.clear();

  // ---- жёлтая полоса: табло ----
  char top[26];
  snprintf(top, sizeof top, "ДИНО  %d", score);
  d.text(0, 1, top, 1);
  char right[14];
  char hearts[6] = "";
  for (int i = 0; i < lives && i < 5; i++) strcat(hearts, "*");
  snprintf(right, sizeof right, "%s  Р%d", hearts, best);
  d.text(OLED_WIDTH - Display::textWidth(right, 1), 1, right, 1);
  g.drawFastHLine(0, 15, OLED_WIDTH, SSD1306_WHITE);

  // ---- небо: солнце, облака плывут медленно, птеродактиль изредка ----
  g.drawBitmap(104, 17, SPR_SUN, 8, 8, SSD1306_WHITE);
  int cloudShift = (int)(dist * 2.0f);
  g.drawBitmap(((20 - cloudShift) % 160 + 160) % 160 - 16, 17, SPR_CLOUD, 16, 6, SSD1306_WHITE);
  g.drawBitmap(((90 - cloudShift) % 160 + 160) % 160 - 16, 20, SPR_CLOUD, 16, 6, SSD1306_WHITE);
  {
    int cycle = ((int)(dist * 6.0f)) % 400;         // раз в "400 шагов" пролетает
    if (cycle < 140) {
      int px = 128 - cycle;
      bool f = ((c.now / 160) & 1);
      g.drawBitmap(px, 18, f ? SPR_PTERO_A : SPR_PTERO_B, 16, 8, SSD1306_WHITE);
    }
  }

  // ---- горизонт, холмы, дорога ----
  for (int x = 0; x < OLED_WIDTH; x += 2) {
    int h = ((x * 5 + (int)(dist * 2)) % 17) / 6;
    if (h) g.drawFastVLine(x, HORIZON - h, h, SSD1306_WHITE);
  }
  g.drawFastHLine(0, HORIZON, OLED_WIDTH, SSD1306_WHITE);
  g.drawLine(laneScreenX(-0.5f, 0), HORIZON, laneScreenX(-0.5f, 1), GROUND, SSD1306_WHITE);
  g.drawLine(laneScreenX(3.5f, 0),  HORIZON, laneScreenX(3.5f, 1),  GROUND, SSD1306_WHITE);
  // разметка между полосами — пунктир, бегущий на нас
  for (int l = 1; l < N; l++) {
    for (int k = 0; k < 7; k++) {
      float t = fmodf(k / 7.0f + dist * 0.2f, 1.0f);
      g.drawPixel(laneScreenX((float)l - 0.5f, t), depthScreenY(t), SSD1306_WHITE);
    }
  }

  // ---- препятствия, дальние первыми ----
  for (int pass = 0; pass < 2; pass++) {
    for (auto& o : obs) {
      if (!o.active) continue;
      float t = (o.y + 1.0f) / (float)N;             // -1..3 -> 0..1
      if (t < 0) t = 0;
      if (t > 1.1f) continue;
      bool far = t < 0.65f;
      if ((pass == 0) != far) continue;
      float tc = t > 1.0f ? 1.0f : t;
      drawObstacle(g, o.kind, laneScreenX((float)o.col, tc), depthScreenY(tc), tc);
    }
  }

  // ---- дино на ближней полосе ----
  uint32_t stride = (uint32_t)(150 - (int)(speed * 12));
  if (stride < 60) stride = 60;
  bool legFrame = ((c.now / stride) & 1);
  bool hurtBlink = (c.now < hurtUntil) && ((c.now / 90) & 1);
  if (!hurtBlink)
    g.drawBitmap(laneScreenX(laneX, 1.0f) - SPR_DINO_W / 2, GROUND - SPR_DINO_H + 1,
                 legFrame ? SPR_DINO_A : SPR_DINO_B, SPR_DINO_W, SPR_DINO_H, SSD1306_WHITE);

  d.flush();
}

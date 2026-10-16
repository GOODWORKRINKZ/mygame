#include "games.h"
#include <math.h>

// ============================================================
//  COLORS — цвета (1 игрок, вся матрица 4x4)
// ============================================================
//  Сверху в случайную колонку падает цветной блок. ЗЕЛЁНАЯ двигает
//  его вправо, СИНЯЯ — влево, он ложится на кучу. Три и больше одного
//  цвета подряд по горизонтали или вертикали исчезают, верхние
//  проседают, и если от этого сложилась новая линия — цепочка и
//  множитель. Новому блоку негде появиться — конец.
//  Рукав зелёного — очки на фоне рекорда, рукав синего — цвет
//  следующего блока и уровень скорости.

static const char* KEY_BEST = "colors";
static const int   N = MATRIX_SIZE;

static inline int mcol(int x) { return MATRIX_COL0 + x; }

static Color blockColor(uint8_t k) {
  switch (k) {
    case 1:  return rgb(255, 30, 30);     // красный
    case 2:  return rgb(255, 200, 0);     // жёлтый
    case 3:  return rgb(0, 200, 255);     // голубой
    case 4:  return rgb(255, 0, 200);     // пурпурный
    default: return 0;
  }
}

void ColorsGame::reset() {
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) grid[r][c] = 0;
  score = 0;
  chain = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  marks = 0;
  clearUntil = 0;
  lastScore = 0;
  lastScoreUntil = 0;
  next = (uint8_t)random(1, 4);
  ph = Ph::Fall;
  spawn();
}

int ColorsGame::palette() const { return score >= 15 ? 4 : 3; }

float ColorsGame::speed() const {
  float v = COLORS_SPEED_START + score * COLORS_SPEED_UP;
  return v > COLORS_SPEED_MAX ? COLORS_SPEED_MAX : v;
}

// Самый нижний свободный ряд колонки (или -1, если колонка полная).
int ColorsGame::restRow(int col) const {
  for (int r = N - 1; r >= 0; r--) if (grid[r][col] == 0) return r;
  return -1;
}

bool ColorsGame::spawn() {
  // колонка, где сверху есть место
  int free_[N], n = 0;
  for (int c = 0; c < N; c++) if (grid[0][c] == 0) free_[n++] = c;
  if (n == 0) return false;
  fcol = free_[random(0, n)];
  fy = -0.9f;
  fkind = next;
  next = (uint8_t)random(1, palette() + 1);
  ph = Ph::Fall;
  return true;
}

// Отметить все линии из 3+ одинаковых; вернуть число клеток.
int ColorsGame::findMatches() {
  marks = 0;
  for (int r = 0; r < N; r++) {
    for (int c = 0; c < N; c++) {
      uint8_t k = grid[r][c];
      if (!k) continue;
      // горизонталь вправо
      int len = 1;
      while (c + len < N && grid[r][c + len] == k) len++;
      if (len >= 3) for (int i = 0; i < len; i++) marks |= (uint16_t)1 << (r * N + c + i);
      // вертикаль вниз
      len = 1;
      while (r + len < N && grid[r + len][c] == k) len++;
      if (len >= 3) for (int i = 0; i < len; i++) marks |= (uint16_t)1 << ((r + i) * N + c);
    }
  }
  int cnt = 0;
  for (int i = 0; i < N * N; i++) if (marks & ((uint16_t)1 << i)) cnt++;
  return cnt;
}

// Убрать отмеченные и уронить всё, что над ними.
void ColorsGame::collapse() {
  for (int c = 0; c < N; c++) {
    int w = N - 1;
    for (int r = N - 1; r >= 0; r--) {
      bool gone = marks & ((uint16_t)1 << (r * N + c));
      if (gone || grid[r][c] == 0) continue;
      grid[w--][c] = grid[r][c];
    }
    while (w >= 0) grid[w--][c] = 0;
  }
  marks = 0;
}

void ColorsGame::land(Ctx& c) {
  int r = restRow(fcol);
  if (r < 0) r = 0;
  grid[r][fcol] = fkind;
  c.out.sfx(Sfx::Tick);
  c.out.buzz(12);
  chain = 0;
  resolve(c);
}

// После приземления или просадки: есть линии — показать и убрать, нет — новый блок.
void ColorsGame::resolve(Ctx& c) {
  int cnt = findMatches();
  if (cnt > 0) {
    chain++;
    int pts = (cnt >= 5) ? 10 : (cnt == 4 ? 6 : 3);
    pts *= chain;
    score += pts;
    lastScore = pts;
    lastScoreUntil = c.now + 900;
    ph = Ph::Clear;
    clearUntil = c.now + COLORS_CLEAR_MS;
    c.out.sfx(chain >= 2 ? Sfx::LevelUp : Sfx::Pickup);
    c.out.buzz(chain >= 2 ? 40 : 20);
    c.fx.spark(0.0f, 10.0f, rgb(255, 255, 255), 0.3f, &ARM_P1);
    return;
  }
  if (!spawn()) {
    over = true;
    overUntil = c.now + 1200;
    newRecord = store.submit(KEY_BEST, score);
    best = store.best(KEY_BEST);
    c.out.sfx(Sfx::GameOver);
    c.fx.flash(rgb(140, 0, 0), 300);
  }
}

void ColorsGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(90, 0, 90), rgb(30, 0, 30));
    char sub[22];
    snprintf(sub, sizeof sub, "ОЧКИ %d", score);
    Ui::banner(c.dpy, "ЗАВАЛИЛО", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  switch (ph) {
    case Ph::Fall: {
      // ---- сдвиг: только если в соседней колонке на этой высоте пусто ----
      int row = (int)floorf(fy + 1.0f);           // ряд, в который блок уже заходит
      if (row < 0) row = 0;
      if (row > N - 1) row = N - 1;
      if (in.p1.pressed && fcol < N - 1 && grid[row][fcol + 1] == 0 &&
          (row == 0 || grid[row - 1][fcol + 1] == 0)) {
        fcol++; c.out.sfx(Sfx::Click);
      }
      if (in.p2.pressed && fcol > 0 && grid[row][fcol - 1] == 0 &&
          (row == 0 || grid[row - 1][fcol - 1] == 0)) {
        fcol--; c.out.sfx(Sfx::Click);
      }
      // ---- падение ----
      fy += speed() * c.dt;
      int rest = restRow(fcol);
      if (rest < 0 || fy >= (float)rest) { land(c); }
      break;
    }
    case Ph::Clear:
      if (c.now >= clearUntil) {
        collapse();
        resolve(c);
      }
      break;
  }

  render(c);
  oled(c);
}

void ColorsGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // ---- куча ----
  for (int r = 0; r < N; r++) {
    for (int x = 0; x < N; x++) {
      uint8_t k = grid[r][x];
      Color col = k ? colScale(blockColor(k), 170) : rgb(3, 3, 5);
      if (k && (marks & ((uint16_t)1 << (r * N + x)))) {
        // исчезающие мигают белым
        uint8_t b = ((c.now / 60) & 1) ? 255 : 90;
        col = colScale(rgb(255, 255, 255), b);
      }
      L.setXY(mcol(x), r, col);
    }
  }

  // ---- падающий блок ----
  if (ph == Ph::Fall) L.addAAXY((float)mcol(fcol), fy, blockColor(fkind));

  // ---- рукав зелёного: очки на фоне рекорда ----
  int target = best > score ? best : (score > 0 ? score : 1);
  float lit = (float)score / (float)target * ARM_P1.len;
  for (int i = 0; i < ARM_P1.len; i++) {
    float f = lit - i;
    uint8_t b = f >= 1.0f ? 150 : (f > 0 ? (uint8_t)(f * 150) : 0);
    L.set(ARM_P1, i, colScale(rgb(255, 255, 255), (uint8_t)(6 + b)));
  }
  if (c.now < lastScoreUntil) L.add(ARM_P1, 0, colScale(rgb(255, 255, 255), 120));

  // ---- рукав синего: следующий цвет (3 точки) и уровень скорости ----
  for (int i = 0; i < 3; i++) L.set(ARM_P2, i, colScale(blockColor(next), 140));
  float k = (speed() - COLORS_SPEED_START) / (COLORS_SPEED_MAX - COLORS_SPEED_START);
  int lvl = (int)(k * (ARM_P2.len - 4) + 0.5f);
  for (int i = 0; i < ARM_P2.len - 4; i++)
    L.set(ARM_P2, 4 + i, (i < lvl) ? rgb(60, 60, 70) : rgb(4, 4, 6));
}

void ColorsGame::oled(Ctx& c) {
  char big[10], l1[24], l2[24], right[8], foot[26];
  snprintf(big, sizeof big, "%d", score);
  float v = speed();
  snprintf(l1, sizeof l1, "СКОРОСТЬ %d.%d  ЦВЕТОВ %d", (int)v, (int)(v * 10) % 10, palette());
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, chain >= 2 ? "x%d" : "", chain);
  if (c.now < lastScoreUntil) snprintf(foot, sizeof foot, "+%d%s", lastScore, chain >= 2 ? "  ЦЕПОЧКА!" : "");
  else                        snprintf(foot, sizeof foot, "%s", hint());
  Ui::solo(c.dpy, name(), right, big, l1, l2, foot);
}

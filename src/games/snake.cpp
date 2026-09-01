#include "games.h"

// ============================================================
//  SNAKE — змейка по кольцу (1 игрок)
// ============================================================
//  Обычная змейка в одну строку выродилась бы в бессмыслицу,
//  поэтому здесь два поворота:
//    * ЗЕЛЁНАЯ не поворачивает, а РАЗВОРАЧИВАЕТ змею — хвост
//      становится головой, и она ползёт в обратную сторону;
//    * каждая съеденная еда роняет на кольцо КАМЕНЬ.
//  Дальше всё решает теснота: камни отрезают куски кольца, и
//  однажды между камнем впереди и собственным хвостом сзади
//  места уже не остаётся. СИНЯЯ ускоряет — быстрее еда, но и
//  меньше времени думать.

static const char* KEY_BEST = "snake";

static inline int wrapIdx(int p) {
  while (p >= LED_COUNT) p -= LED_COUNT;
  while (p < 0)          p += LED_COUNT;
  return p;
}

bool SnakeGame::occupied(int p) const {
  if (rock[p]) return true;
  for (int i = 0; i < len; i++) if (body[i] == p) return true;
  return false;
}

// Дуга, в которой змея сейчас заперта: от головы идём в обе стороны, пока
// не упрёмся в камень. Через камень змея пройти не может никогда, поэтому
// всё, что за ним, для неё не существует — ни еду там ставить нельзя,
// ни считать это место запасом свободы.
int SnakeGame::arcCells(int* out, int cap) const {
  int n = 0;
  int h = body[0];
  if (cap <= 0 || rock[h]) return 0;
  out[n++] = h;

  for (int d = 1; d < LED_COUNT && n < cap; d++) {
    int p = wrapIdx(h + d);
    if (rock[p]) break;
    out[n++] = p;
  }
  for (int d = 1; d < LED_COUNT && n < cap; d++) {
    int p = wrapIdx(h - d);
    if (rock[p]) break;
    // на кольце без камней обе ветки встретятся — второй раз не считаем
    bool dup = false;
    for (int i = 0; i < n; i++) if (out[i] == p) { dup = true; break; }
    if (dup) break;
    out[n++] = p;
  }
  return n;
}

void SnakeGame::placeFood() {
  int arc[LED_COUNT];
  int free_[LED_COUNT];
  int n = arcCells(arc, LED_COUNT);
  int m = 0;
  for (int i = 0; i < n; i++)
    if (!occupied(arc[i])) free_[m++] = arc[i];

  food = (m > 0) ? free_[random(0, m)] : -1;
}

void SnakeGame::placeRock() {
  int arc[LED_COUNT], test[LED_COUNT];
  int n = arcCells(arc, LED_COUNT);
  if (n <= 0) return;

  for (int tries = 0; tries < 60; tries++) {
    int p = arc[random(0, n)];             // камень имеет смысл только в своей дуге
    if (occupied(p) || p == food) continue;

    // не роняем камень прямо под нос: должно остаться время среагировать
    int d = wrapIdx(p - body[0]);
    if (d > LED_COUNT / 2) d = LED_COUNT - d;
    if (d < 3) continue;

    // камень режет дугу надвое, и половина за ним пропадает. Проверяем,
    // что змее осталось где жить: тело плюс запас на следующую еду.
    rock[p] = true;
    if (arcCells(test, LED_COUNT) < len + 4) {
      rock[p] = false;
      continue;
    }
    return;
  }
}

void SnakeGame::reset() {
  len = SNAKE_START_LEN;
  dir = 1;
  int start = LED_COUNT / 2;
  for (int i = 0; i < len; i++) body[i] = (int8_t)wrapIdx(start - i);
  for (int i = 0; i < LED_COUNT; i++) rock[i] = false;
  stepMs = SNAKE_STEP_START;
  nextStep = millis() + stepMs;
  score = 0;
  best = store.best(KEY_BEST);
  newRecord = false;
  over = false;
  overUntil = 0;
  flipUntil = 0;
  placeFood();
}

void SnakeGame::die(Ctx& c) {
  over = true;
  overUntil = c.now + 1200;
  newRecord = store.submit(KEY_BEST, score);
  best = store.best(KEY_BEST);
  c.out.sfx(Sfx::GameOver);
  c.out.buzz(160);
  c.fx.burst((float)body[0], rgb(255, 60, 0), 14, 26.0f);
  c.fx.flash(rgb(140, 0, 0), 300);
}

void SnakeGame::step(Ctx& c) {
  int next = wrapIdx(body[0] + dir);

  if (rock[next]) { die(c); return; }
  // в клетку хвоста заходить можно: он как раз её освобождает
  for (int i = 0; i < len - 1; i++) {
    if (body[i] == next) { die(c); return; }
  }

  int oldTail = body[len - 1];
  for (int i = len - 1; i > 0; i--) body[i] = body[i - 1];
  body[0] = (int8_t)next;

  if (next == food) {
    score++;
    if (len < SNAKE_MAX_LEN) {
      body[len] = (int8_t)oldTail;
      len++;
    }
    c.out.sfx(Sfx::Pickup);
    c.out.buzz(20);
    c.fx.spark((float)next, 8.0f, rgb(255, 220, 0), 0.3f);
    placeRock();
    placeFood();
    stepMs = (uint16_t)(stepMs * SNAKE_STEP_UP);
    if (stepMs < SNAKE_STEP_MIN) stepMs = SNAKE_STEP_MIN;
  }
}

void SnakeGame::update(const Inputs& in, Ctx& c) {
  // ---- экран проигрыша ----
  if (over) {
    if (c.now >= overUntil && (in.p1.pressed || in.p2.pressed)) { reset(); return; }
    if (c.now >= overUntil + 6000) { reset(); return; }
    Bg::confetti(c.leds, c.now, rgb(0, 70, 0), rgb(20, 0, 0));
    char sub[22];
    snprintf(sub, sizeof sub, "ЕДЫ %d  ДЛИНА %d", score, len);
    Ui::banner(c.dpy, "КОНЕЦ ИГРЫ", sub,
               newRecord ? "НОВЫЙ РЕКОРД!" : "ЖМИ ДЛЯ ПОВТОРА");
    return;
  }

  // ---- разворот: хвост становится головой ----
  if (in.p1.pressed) {
    for (int i = 0; i < len / 2; i++) {
      int8_t t = body[i];
      body[i] = body[len - 1 - i];
      body[len - 1 - i] = t;
    }
    dir = -dir;
    flipUntil = c.now + 160;
    c.out.tone(880, 30);
  }

  // ---- шаг по таймеру, синяя кнопка ускоряет вдвое ----
  if (c.now >= nextStep) {
    step(c);
    if (over) return;
    uint16_t gap = in.p2.held ? (uint16_t)(stepMs / 2) : stepMs;
    if (gap < 45) gap = 45;
    nextStep = c.now + gap;
  }

  render(c);
  oled(c);
}

void SnakeGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  // камни
  for (int i = 0; i < LED_COUNT; i++)
    if (rock[i]) L.set(i, rgb(90, 25, 0));

  // тело: к хвосту темнее
  for (int i = len - 1; i >= 1; i--) {
    uint8_t b = (uint8_t)(40 + (uint16_t)(len - i) * 120 / len);
    L.set(body[i], colScale(COL_P1, b));
  }

  // еда пульсирует
  if (food >= 0) {
    uint8_t b = (uint8_t)(120 + sin8((uint8_t)(c.now / 4)) / 2);
    L.set(food, colScale(rgb(255, 210, 0), b));
  }

  // голова
  L.set(body[0], rgb(220, 255, 220));
  if (c.now < flipUntil) L.add(body[0], rgb(255, 255, 255));
}

void SnakeGame::oled(Ctx& c) {
  int rocks = 0;
  for (int i = 0; i < LED_COUNT; i++) if (rock[i]) rocks++;

  char big[10], l1[24], l2[24], right[8];
  snprintf(big, sizeof big, "%d", score);
  snprintf(l1, sizeof l1, "ДЛИНА %d  КАМНИ %d", len, rocks);
  snprintf(l2, sizeof l2, "РЕКОРД %d", best);
  snprintf(right, sizeof right, "Д%d", len);

  Ui::solo(c.dpy, name(), right, big, l1, l2, hint());
}

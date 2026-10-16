#include "games.h"
#include <math.h>

// ============================================================
//  ORBIT — орбита (2 игрока)
// ============================================================
//  Первая игра, которая видит ленту не линией, а тем, что она есть
//  физически: рукав зелёного, матрица 4x4 в центре, рукав синего.
//
//  Комета вылетает от кнопки одного игрока, бежит по его рукаву,
//  влетает в кольцо матрицы, делает там 0..ORBIT_LAPS_MAX кругов (в
//  любую сторону) и вылетает в рукав соперника. Сердцевина матрицы
//  подсказывает, сколько кругов ещё осталось. Отбить комету можно
//  только когда она в последних ORBIT_ZONE клетках у твоей кнопки:
//  тогда она летит обратно, быстрее и с новым числом кругов.
//  Нажал вхолостую — короткий ступор. Прозевал — раунд соперника.
//
//  Вся геометрия — Route из canvas.h: три куска (рукав, кольцо с
//  кругами, рукав) склеены в одну координату 0..length().

void OrbitGame::reset() {
  m.reset();
  newRound(millis());
}

// Маршрут от игрока fromP к сопернику: его рукав -> кольцо (lapsN
// полных кругов плюс полкольца до чужих ворот) -> чужой рукав.
void OrbitGame::buildRoute(int fromP, int lapsN, int dirCw) {
  route.clear();
  const Path& armFrom = (fromP == 1) ? ARM_P1 : ARM_P2;
  const Path& armTo   = (fromP == 1) ? ARM_P2 : ARM_P1;
  int gateFrom = (fromP == 1) ? RING_GATE_P1 : RING_GATE_P2;
  int gateTo   = (fromP == 1) ? RING_GATE_P2 : RING_GATE_P1;

  // сколько клеток по кольцу от своих ворот до чужих в выбранную сторону
  int half = (dirCw > 0) ? (gateTo - gateFrom) : (gateFrom - gateTo);
  while (half < 0) half += RING.len;

  route.add(armFrom, 0, armFrom.len, +1);                       // от кнопки к матрице
  route.add(RING, gateFrom, RING.len * lapsN + half + 1, dirCw); // по кольцу до чужих ворот
  route.add(armTo, armTo.len - 1, armTo.len, -1);               // от матрицы к чужой кнопке
  laps = lapsN;
}

void OrbitGame::serve(int fromP, uint32_t now) {
  from = fromP;
  buildRoute(from, (int)random(0, ORBIT_LAPS_MAX + 1), random(2) ? 1 : -1);
  pos = ORBIT_ZONE - 0.5f;          // комета уже выходит из зоны подающего
  serveAt = now + 900;
}

void OrbitGame::newRound(uint32_t now) {
  speed = ORBIT_SPEED_START;
  volleys = 0;
  stun[0] = stun[1] = 0;
  event = "";
  serve(random(2) ? 1 : 2, now);
}

// Комета в последних ORBIT_ZONE клетках маршрута — у кнопки принимающего.
bool OrbitGame::inZone() const {
  return pos >= (float)(route.length() - ORBIT_ZONE) - 0.5f;
}

// Сколько полных кругов по кольцу ещё впереди (0, если комета уже
// вылетает или ещё не влетела).
int OrbitGame::lapsLeft() const {
  int seg = route.segOf((int)pos);
  if (seg != 1) return (seg < 1) ? laps : 0;
  float ringEnd = (float)(route.segStart(1) + route.segs[1].count);
  float left = ringEnd - pos - 1.0f;
  return left > 0 ? (int)(left / (float)RING.len) : 0;
}

void OrbitGame::update(const Inputs& in, Ctx& c) {
  if (!m.playing()) {
    if (m.tick(c.now)) {
      newRound(c.now);
    } else {
      drawCelebration(c, m.winner, m.over());
      oled(c);
      return;
    }
  }

  int receiver = 3 - from;

  // ---- подача: комета висит у подающего и мигает ----
  if (serveAt) {
    if (c.now >= serveAt) { serveAt = 0; c.out.sfx(Sfx::Beep); }
    render(c);
    oled(c);
    return;
  }

  // ---- ввод ----
  const BtnState* btn[2] = { &in.p1, &in.p2 };
  for (int p = 0; p < 2; p++) {
    if (!btn[p]->pressed || c.now < stun[p]) continue;
    bool mine = (p + 1 == receiver) && inZone();
    if (mine) {
      // отбил: маршрут разворачивается, координата зеркалится
      float L = (float)route.length();
      const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
      c.fx.burst(L - 1.0f - pos, p == 0 ? COL_P1 : COL_P2, 6, 16.0f, &arm);
      c.out.tone((uint16_t)(700 + volleys * 40), 45);
      c.out.buzz(35);
      volleys++;
      speed *= ORBIT_SPEED_MUL;
      from = p + 1;
      buildRoute(from, (int)random(0, ORBIT_LAPS_MAX + 1), random(2) ? 1 : -1);
      pos = L - 1.0f - pos;
      if (pos < 0) pos = 0;
      event = (p == 0) ? "ЗЕЛЕНЫЙ ОТБИЛ" : "СИНИЙ ОТБИЛ";
      receiver = 3 - from;
    } else {
      stun[p] = c.now + ORBIT_STUN_MS;
      c.out.sfx(Sfx::Error);
    }
  }

  // ---- полёт ----
  pos += speed * c.dt;
  if (pos > (float)route.length() - 0.5f) {
    // принимающий прозевал
    const Path& arm = (receiver == 1) ? ARM_P1 : ARM_P2;
    c.fx.burst(0.0f, rgb(255, 120, 0), 12, 24.0f, &arm);
    c.fx.flash(from == 1 ? COL_P1 : COL_P2, 250);
    c.out.sfx(Sfx::Explode);
    event = (receiver == 1) ? "ЗЕЛЕНЫЙ ПРОЗЕВАЛ" : "СИНИЙ ПРОЗЕВАЛ";
    m.winRound(from, c.now, c);
  }

  render(c);
  oled(c);
}

void OrbitGame::render(Ctx& c) {
  LedStrip& L = c.leds;
  L.clear();

  int receiver = 3 - from;
  bool zone = inZone() && !serveAt;

  // ---- рукава: тусклые своим цветом, зона у кнопки чуть ярче.
  //      Когда комета в зоне принимающего, зона горит в полную силу. ----
  for (int p = 0; p < 2; p++) {
    const Path& arm = (p == 0) ? ARM_P1 : ARM_P2;
    Color col = (p == 0) ? COL_P1 : COL_P2;
    bool hot = zone && (p + 1 == receiver);
    for (int i = 0; i < arm.len; i++) {
      uint8_t b = (i < ORBIT_ZONE) ? (hot ? 150 : 40) : 14;
      L.set(arm, i, colScale(col, b));
    }
    // ступор: рукав нервно мигает красным
    if (c.now < stun[p]) {
      uint8_t f = ((c.now / 60) & 1) ? 120 : 20;
      for (int i = 0; i < ORBIT_ZONE; i++) L.add(arm, i, rgb(f, 0, 0));
    }
  }

  // ---- кольцо: тусклая дорожка ----
  L.fill(RING, rgb(5, 5, 8));

  // ---- сердцевина: сколько кругов впереди ----
  int ll = lapsLeft();
  bool inRing = (route.segOf((int)pos) == 1);
  Color coreCol = (ll >= 2) ? rgb(255, 40, 0) : (ll == 1 ? rgb(255, 170, 0) : rgb(0, 200, 120));
  uint8_t cb = inRing ? (uint8_t)(90 + sin8((uint8_t)(c.now / 3)) / 2) : 18;
  L.fill(CORE, colScale(coreCol, cb));

  // ---- комета с хвостом ----
  Color head = colLerp(rgb(255, 120, 0), rgb(255, 255, 220), sin8((uint8_t)(c.now / 4)));
  if (serveAt) {
    uint8_t pulse = sin8((uint8_t)(c.now / 2));
    L.addAA(route, pos, colScale(head, (uint8_t)(60 + pulse / 2)));
  } else {
    L.addAA(route, pos, head);
    L.addAA(route, pos - 1.0f, colScale(head, 90));
    L.addAA(route, pos - 2.0f, colScale(head, 30));
  }
}

void OrbitGame::oled(Ctx& c) {
  char b1[6], b2[6], s1[16], s2[16], foot[32];
  snprintf(b1, sizeof b1, "%d", m.score[0]);
  snprintf(b2, sizeof b2, "%d", m.score[1]);

  int receiver = 3 - from;
  bool zone = inZone() && !serveAt;
  for (int p = 0; p < 2; p++) {
    char* dst = (p == 0) ? s1 : s2;
    if (c.now < stun[p])                 strcpy(dst, "ОГЛУШЕН");
    else if (p + 1 == receiver && zone)  strcpy(dst, "БЕЙ!");
    else if (p + 1 == receiver)          strcpy(dst, "К ТЕБЕ");
    else                                 strcpy(dst, "ЖДИ");
  }

  Panel l, r;
  // Зелёный (winner=1) — справа, синий (winner=2) — слева.
  l.name = "СИНИЙ";   l.big = b2; l.sub = s2;
  l.dots = m.score[1]; l.dotsMax = ROUNDS_TO_WIN; l.active = (receiver == 2 && !m.winner);
  r.name = "ЗЕЛЕНЫЙ"; r.big = b1; r.sub = s1;
  r.dots = m.score[0]; r.dotsMax = ROUNDS_TO_WIN; r.active = (receiver == 1 && !m.winner);

  const char* f = foot;
  if (m.over())      f = (m.winner == 1) ? "ЗЕЛЕНЫЙ ПОБЕДИЛ" : "СИНИЙ ПОБЕДИЛ";
  else if (m.winner) f = event;
  else if (serveAt)  f = "ПРИГОТОВЬСЯ...";
  else snprintf(foot, sizeof foot, "КРУГОВ %d  X%d", lapsLeft(), volleys);

  drawMatchOled(c, name(), m, l, r, f);
}

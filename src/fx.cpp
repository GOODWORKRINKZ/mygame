#include "fx.h"
#include <math.h>

// ============================================================
//  Fx — частицы, вспышки, ударные волны
// ============================================================

void Fx::reset() {
  for (int i = 0; i < MAX_PARTICLES; i++) _p[i].active = false;
  _flashDur = 0;
  _shockDur = 0;
}

Particle* Fx::freeSlot() {
  // Свободный слот, а если все заняты — самая "мёртвая" частица.
  Particle* weakest = &_p[0];
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!_p[i].active) return &_p[i];
    if (_p[i].life < weakest->life) weakest = &_p[i];
  }
  return weakest;
}

void Fx::spark(float pos, float vel, Color c, float lifeSec) {
  Particle* p = freeSlot();
  p->pos = pos;
  p->vel = vel;
  p->life = 1.0f;
  p->decay = lifeSec > 0.01f ? (1.0f / lifeSec) : 4.0f;
  p->drag = 1.6f;
  p->color = c;
  p->active = true;
}

void Fx::burst(float pos, Color c, uint8_t n, float speed) {
  for (uint8_t i = 0; i < n; i++) {
    float k = ((float)random(30, 130)) / 100.0f;     // разброс скоростей
    float v = speed * k * ((i & 1) ? 1.0f : -1.0f);
    spark(pos, v, c, 0.35f + (float)random(0, 40) / 100.0f);
  }
}

void Fx::jet(float pos, int dir, Color c, uint8_t n, float speed) {
  for (uint8_t i = 0; i < n; i++) {
    float k = ((float)random(40, 120)) / 100.0f;
    spark(pos, speed * k * (dir >= 0 ? 1.0f : -1.0f), c, 0.25f);
  }
}

void Fx::flash(Color c, uint16_t durMs) {
  _flashColor = c;
  _flashStart = millis();
  _flashDur = durMs;
}

void Fx::shock(float pos, Color c, float speed, uint16_t durMs) {
  _shockColor = c;
  _shockPos = pos;
  _shockR = 0;
  _shockSpeed = speed;
  _shockStart = millis();
  _shockDur = durMs;
}

void Fx::update(float dt) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle& p = _p[i];
    if (!p.active) continue;
    p.pos += p.vel * dt;
    p.vel -= p.vel * p.drag * dt;
    p.life -= p.decay * dt;
    if (p.life <= 0.0f || p.pos < -1.5f || p.pos > LED_COUNT + 0.5f) p.active = false;
  }
  if (_shockDur) _shockR += _shockSpeed * dt;
}

void Fx::draw(LedStrip& leds) {
  uint32_t now = millis();

  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle& p = _p[i];
    if (!p.active) continue;
    uint8_t b = (uint8_t)(p.life * 255.0f);
    leds.addAA(p.pos, colScale(p.color, b));
  }

  if (_shockDur) {
    uint32_t el = now - _shockStart;
    if (el >= _shockDur) {
      _shockDur = 0;
    } else {
      uint8_t b = (uint8_t)(255 - (el * 255) / _shockDur);
      leds.addAA(_shockPos + _shockR, colScale(_shockColor, b));
      leds.addAA(_shockPos - _shockR, colScale(_shockColor, b));
    }
  }

  if (_flashDur) {
    uint32_t el = now - _flashStart;
    if (el >= _flashDur) {
      _flashDur = 0;
    } else {
      uint8_t b = (uint8_t)(255 - (el * 255) / _flashDur);
      leds.addAll(colScale(_flashColor, b));
    }
  }
}

bool Fx::busy() const {
  if (_flashDur || _shockDur) return true;
  for (int i = 0; i < MAX_PARTICLES; i++) if (_p[i].active) return true;
  return false;
}

// ============================================================
//  Фоновые "обои"
// ============================================================
namespace Bg {

void rainbow(LedStrip& leds, uint32_t now, uint8_t speed, uint8_t val) {
  uint8_t base = (uint8_t)((now * speed) / 1000);
  for (int i = 0; i < LED_COUNT; i++)
    leds.set(i, colHsv((uint8_t)(base + i * 6), 255, val));
}

void breathe(LedStrip& leds, uint32_t now, Color c, uint16_t periodMs) {
  uint8_t phase = (uint8_t)(((now % periodMs) * 255) / periodMs);
  uint8_t b = (uint8_t)(20 + (uint16_t)sin8(phase) * 200 / 255);
  leds.fillAll(colScale(c, b));
}

void comet(LedStrip& leds, uint32_t now, Color c, uint16_t periodMs, uint8_t tail) {
  float head = (float)(now % periodMs) * (LED_COUNT + tail) / (float)periodMs - tail;
  for (uint8_t k = 0; k < tail; k++) {
    float p = head - k;
    if (p < 0 || p > LED_COUNT - 1) continue;
    uint8_t b = (uint8_t)(255 - (255 * k) / tail);
    leds.addAA(p, colScale(c, b));
  }
}

void confetti(LedStrip& leds, uint32_t now, Color a, Color b) {
  (void)now;
  leds.fade(200);
  if (random(100) < 45)
    leds.add((int)random(LED_COUNT), random(2) ? a : b);
}

void sparkleFill(LedStrip& leds, Color base, uint8_t chancePercent, Color sparkle) {
  leds.fillAll(base);
  for (int i = 0; i < LED_COUNT; i++)
    if ((int)random(100) < chancePercent) leds.set(i, sparkle);
}

}  // namespace Bg

#pragma once
#include <Arduino.h>
#include "hardware.h"

// ============================================================
//  Слой спецэффектов поверх ленты.
//  Живёт отдельно от игр: игра говорит "тут взрыв" / "тут искры",
//  а Fx сам их анимирует и дорисовывает в кадр.
// ============================================================

struct Particle {
  float pos;      // положение в пикселях (может быть дробным)
  float vel;      // пикселей в секунду
  float life;     // 0..1, доживает до нуля
  float decay;    // сколько life теряется за секунду
  float drag;     // торможение
  Color color;
  bool  active;
};

class Fx {
public:
  static const int MAX_PARTICLES = 28;

  void reset();

  // Одна искра.
  void spark(float pos, float vel, Color c, float lifeSec = 0.5f);
  // Круговой "взрыв": n искр в обе стороны.
  void burst(float pos, Color c, uint8_t n = 8, float speed = 14.0f);
  // Направленный шлейф (например, из дула).
  void jet(float pos, int dir, Color c, uint8_t n = 4, float speed = 10.0f);

  // Полноэкранная вспышка, гаснет за durMs.
  void flash(Color c, uint16_t durMs = 180);
  // Бегущая волна от точки в обе стороны (ударная волна).
  void shock(float pos, Color c, float speed = 40.0f, uint16_t durMs = 400);

  void update(float dt);
  void draw(LedStrip& leds);

  bool busy() const;

private:
  Particle _p[MAX_PARTICLES];

  Color    _flashColor = 0;
  uint32_t _flashStart = 0, _flashDur = 0;

  Color    _shockColor = 0;
  float    _shockPos = 0, _shockR = 0, _shockSpeed = 0;
  uint32_t _shockStart = 0, _shockDur = 0;

  Particle* freeSlot();
};

// ============================================================
//  Готовые фоновые "обои" для ленты (меню, паузы, победы).
// ============================================================
namespace Bg {
  void rainbow(LedStrip& leds, uint32_t now, uint8_t speed = 40, uint8_t val = 90);
  void breathe(LedStrip& leds, uint32_t now, Color c, uint16_t periodMs = 2400);
  void comet(LedStrip& leds, uint32_t now, Color c, uint16_t periodMs = 1500,
             uint8_t tail = 7);
  void confetti(LedStrip& leds, uint32_t now, Color a, Color b);
  void sparkleFill(LedStrip& leds, Color base, uint8_t chancePercent, Color sparkle);
}

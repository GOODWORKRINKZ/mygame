#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "led_map.h"
#include "log.h"

// ============================================================
//  Лента с логической адресацией (поверх LedMap)
// ============================================================
class LedStrip {
public:
  LedMap map;

  void begin();
  void clear();
  void show();

  void setLogical(uint8_t logical, uint8_t r, uint8_t g, uint8_t b);
  void setLogical(uint8_t logical, uint32_t color);
  void fillLogical(uint8_t from, uint8_t to, uint32_t color);
  void fillAll(uint32_t color);

  // Диагностика: мигает всеми 32 светодиодами красным/зелёным/синим/белым
  // по 500 мс каждый. Используется для проверки порядка цветов и питания.
  void colorTest();
  // Диагностика: зажигает по одному светодиоду за раз (0, 1, 2, ... 31),
  // каждая позиция горит 100 мс. Позволяет увидеть, какие физические
  // индексы реально работают и в каком порядке.
  void oneByOneTest();

  uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return _strip.Color(r, g, b); }
  uint32_t hsv(uint16_t hue, uint8_t sat = 255, uint8_t val = 255);

private:
  // Если в config.h определён PIN_LED_ALT, используем его (для диагностики),
  // иначе — основной PIN_LED.
#ifdef PIN_LED_ALT
  Adafruit_NeoPixel _strip{LedMap::COUNT, PIN_LED_ALT, NEO_GRB + NEO_KHZ800};
  static const uint8_t ACTIVE_PIN = PIN_LED_ALT;
#else
  Adafruit_NeoPixel _strip{LedMap::COUNT, PIN_LED, NEO_GRB + NEO_KHZ800};
  static const uint8_t ACTIVE_PIN = PIN_LED;
#endif
};

// ============================================================
//  OLED 128x64
// ============================================================
class Display {
public:
  void begin();
  void splash(const char* title, const char* subtitle);
  void gameFrame(const char* title, int s1, int s2, const char* line1, const char* line2);
  void flush();   // вывод кадра с троттлингом (анти-мерцание)
  void bootFrame(uint8_t phase);   // кадры заставки при включении (0..2)
  void textCentered(int y, const char* s, uint8_t size = 1);

private:
  Adafruit_SSD1306 _oled{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
  uint32_t _lastFlush = 0;
};

// ============================================================
//  Звук (бузер) + вибрация
// ============================================================
struct Note {
  uint16_t freq;
  uint16_t durMs;
};

class Output {
public:
  void begin();
  void update();                       // вызывать каждый цикл loop()

  void tone(uint16_t freq, uint16_t durMs);
  void melody(const Note* notes, uint8_t count);
  void beep();                         // короткий высокий
  void click();                        // очень короткий
  void buzz(uint16_t durMs);           // импульс вибрации
  void winSound();
  void loseSound();
  void startupFanfare();               // приветствие при включении

private:
  uint32_t _toneEnd = 0;
  uint32_t _vibroEnd = 0;
  const Note* _melody = nullptr;
  uint8_t _melCount = 0;
  uint8_t _melIdx = 0;
};

// ============================================================
//  Кнопка (активный низкий уровень + INPUT_PULLUP)
// ============================================================
class Button {
public:
  void begin(uint8_t pin);
  void update();

  bool pressed() const { return _pressed; }   // фронт "нажали"
  bool released() const { return _released; } // фронт "отпустили"
  bool held() const { return _raw; }          // удерживается
  bool longPress();                           // срабатывает один раз после удержания

private:
  uint8_t _pin = 0;
  bool _raw = false;
  bool _lastDebounceRaw = false;
  bool _pressed = false;
  bool _released = false;
  bool _longFired = false;
  bool _longFlag = false;
  uint32_t _lastDebounceTime = 0;
  uint32_t _pressStart = 0;
};

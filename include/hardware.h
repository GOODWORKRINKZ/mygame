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
//  Цвет — упакованный 0x00RRGGBB. Работаем в своём буфере, а в
//  NeoPixel отдаём только готовый кадр (с гамма-коррекцией).
// ============================================================
typedef uint32_t Color;

static inline Color rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint8_t colR(Color c) { return (c >> 16) & 0xFF; }
static inline uint8_t colG(Color c) { return (c >> 8) & 0xFF; }
static inline uint8_t colB(Color c) { return c & 0xFF; }

// Умножение цвета на коэффициент 0..255 (затухание).
Color colScale(Color c, uint8_t s);
// Линейная интерполяция цветов, t = 0..255.
Color colLerp(Color a, Color b, uint8_t t);
// HSV -> RGB, hue 0..255.
Color colHsv(uint8_t hue, uint8_t sat = 255, uint8_t val = 255);

// Целочисленный синус: вход 0..255 (полный оборот), выход 0..255.
uint8_t sin8(uint8_t theta);
// "Дыхание": треугольная волна 0..255.
uint8_t tri8(uint8_t theta);

// ---- Командные цвета (используются во всех играх и на OLED) ----
#define COL_P1        rgb(0, 235, 60)      // зелёный игрок
#define COL_P1_DIM    rgb(0, 60, 16)
#define COL_P2        rgb(30, 90, 255)     // синий игрок
#define COL_P2_DIM    rgb(6, 22, 70)
#define COL_BALL      rgb(255, 255, 255)
#define COL_NEUTRAL   rgb(40, 40, 46)

// ============================================================
//  Лента: логический кадровый буфер поверх физической ленты
// ============================================================
//  Игры работают только с логическими индексами 0..LED_COUNT-1.
//  При выводе кадра каждый логический пиксель уходит на свой
//  физический адрес из LED_LOGICAL_MAP, а физические светодиоды,
//  которых в таблице нет, заливаются цветом стен.
// ============================================================
class LedStrip {
public:
  void begin();

  // ---- работа с кадром ----
  void clear();                       // погасить буфер
  void fade(uint8_t keep);            // умножить весь кадр на keep/255 (хвосты)
  void show();                        // гамма + вывод на ленту

  void set(int logical, Color c);     // записать поверх
  void add(int logical, Color c);     // сложить (аддитивное смешивание)
  void addAA(float pos, Color c);     // субпиксельная точка со сглаживанием
  void fill(int from, int to, Color c);
  void fillAll(Color c);
  void addAll(Color c);

  Color get(int logical) const;

  // ---- карта ----
  uint8_t physicalOf(int logical) const;        // логический -> физический
  bool    isWall(int physical) const;           // этот физический вне игры?
  void    setWall(Color c) { _wall = c; }       // перекрасить стены
  Color   wall() const { return _wall; }
  void    resetWall();                          // вернуть цвет из led_map.h

  // Совместимость со старым кодом
  Color rgb(uint8_t r, uint8_t g, uint8_t b) const { return ::rgb(r, g, b); }
  Color hsv(uint8_t h, uint8_t s = 255, uint8_t v = 255) const { return colHsv(h, s, v); }

  // ---- диагностика ----
  void colorTest();          // R/G/B/W по всей ФИЗИЧЕСКОЙ ленте (проверка железа)
  void physicalScanTest();   // бегущий пиксель по ФИЗИЧЕСКИМ номерам (сбор карты)
  void oneByOneTest();       // бегущий пиксель по ЛОГИЧЕСКИМ номерам (проверка карты)

private:
  static const uint8_t NO_LOGICAL = 0xFF;

  void rawPixel(int physical, Color c);   // напрямую в ленту, мимо карты
  void rawFill(Color c);

  Color   _buf[LED_COUNT];                     // логический кадр
  Color   _wall = 0;                           // цвет стен
  uint8_t _phys2log[LED_PHYSICAL_COUNT];       // физический -> логический (или NO_LOGICAL)
  Adafruit_NeoPixel _strip{LED_PHYSICAL_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800};
};

// ============================================================
//  OLED 128x64. Наружу отдаём сам Adafruit_GFX — рисование
//  интерфейса живёт в ui.h/ui.cpp.
// ============================================================
class Display {
public:
  void begin();
  Adafruit_SSD1306& gfx() { return _oled; }

  void clear();
  void flush();        // вывод кадра не чаще OLED_MIN_MS
  void flushNow();     // вывод немедленно (заставки, смена экрана)

  void text(int x, int y, const char* s, uint8_t size = 1);
  void textCentered(int y, const char* s, uint8_t size = 1);
  // Центрирование внутри произвольной колонки (для сплит-экрана).
  void textCenteredIn(int x0, int x1, int y, const char* s, uint8_t size = 1);
  static int textWidth(const char* s, uint8_t size = 1);

  bool ok() const { return _ok; }

private:
  Adafruit_SSD1306 _oled{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
  uint32_t _lastFlush = 0;
  bool _ok = false;
};

// ============================================================
//  Звук (бузер) + вибрация
// ============================================================
struct Note {
  uint16_t freq;   // Гц, 0 = пауза
  uint16_t durMs;
};

// Каталог звуковых эффектов
enum class Sfx : uint8_t {
  Click, Beep, Select, Back, Error,
  Shoot, ShootCharged, Hit, Explode, Bounce, Twang,
  Tick, Pickup, LevelUp, Win, Lose, Fanfare, GameOver, Countdown
};

class Output {
public:
  void begin();
  void update();                       // вызывать каждый цикл loop()

  void tone(uint16_t freq, uint16_t durMs);   // одиночный тон
  void slide(uint16_t from, uint16_t to, uint16_t durMs);  // плавный "вжух"
  void melody(const Note* notes, uint8_t count);
  void sfx(Sfx id);
  void silence();

  void buzz(uint16_t durMs);           // одиночный импульс вибрации
  void buzzPattern(uint8_t pulses, uint16_t onMs, uint16_t offMs);

  // Совместимость
  void beep()  { sfx(Sfx::Beep); }
  void click() { sfx(Sfx::Click); }
  void winSound()  { sfx(Sfx::Win); }
  void loseSound() { sfx(Sfx::Lose); }
  void startupFanfare() { sfx(Sfx::Fanfare); }

private:
  void writeTone(uint16_t freq);

  uint32_t _toneEnd = 0;
  const Note* _melody = nullptr;
  uint8_t _melCount = 0;
  uint8_t _melIdx = 0;

  // Плавный слайд частоты
  bool     _sliding = false;
  uint32_t _slideStart = 0, _slideDur = 0;
  uint16_t _slideFrom = 0, _slideTo = 0;

  // Вибро
  uint32_t _vibroNext = 0;
  uint8_t  _vibroLeft = 0;
  uint16_t _vibroOn = 0, _vibroOff = 0;
  bool     _vibroState = false;
};

// ============================================================
//  Кнопка (замыкает на GND, внутренняя подтяжка INPUT_PULLUP)
// ============================================================
class Button {
public:
  void begin(uint8_t pin);
  void update();

  bool pressed()  const { return _pressed; }    // фронт "нажали"
  bool released() const { return _released; }   // фронт "отпустили"
  bool held()     const { return _raw; }        // удерживается прямо сейчас
  uint32_t heldMs() const;                      // сколько удерживается, мс
  uint32_t lastHeldMs() const { return _lastHeld; } // длительность последнего нажатия
  bool longPress();                             // один раз после BTN_LONG_MS
  // Автоповтор при удержании — для навигации по меню.
  bool repeat(uint16_t firstMs = 450, uint16_t rateMs = 130);

private:
  uint8_t  _pin = 0;
  bool     _raw = false;
  bool     _lastDebounceRaw = false;
  bool     _pressed = false;
  bool     _released = false;
  bool     _longFired = false;
  bool     _longFlag = false;
  uint32_t _lastDebounceTime = 0;
  uint32_t _pressStart = 0;
  uint32_t _lastHeld = 0;
  uint32_t _nextRepeat = 0;
};

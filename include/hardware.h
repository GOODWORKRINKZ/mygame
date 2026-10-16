#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "led_map.h"
#include "canvas.h"
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
//  Лента: кадровый буфер поверх физической ленты
// ============================================================
//  Кадр хранится по ФИЗИЧЕСКИМ номерам, а наружу торчат три
//  системы координат:
//    * логическая линия 0..LED_COUNT-1 (set/add/addAA/fill) — так
//      работают все одномерные игры, как и раньше;
//    * пути (Path/Route из canvas.h) — рукава, кольцо, маршруты;
//    * сетка (setXY/addXY/addAAXY) — матрица как картинка.
//  Все три рисуют в один и тот же кадр и могут пересекаться.
//
//  Стены. В обычном режиме физические светодиоды, которых нет в
//  логической линии, при clear()/fade() заливаются цветом стен, и
//  одномерная игра видит привычную рамку. Игра, которой нужна вся
//  лента (fullCanvas() == true), стен не получает: после clear()
//  у неё все LED_PHYSICAL_COUNT светодиодов чёрные и свои.
// ============================================================
class LedStrip {
public:
  void begin();

  // ---- кадр целиком ----
  void clear();                       // погасить (в обычном режиме стены остаются)
  void fade(uint8_t keep);            // умножить кадр на keep/255 (хвосты)
  void show();                        // гамма + вывод на ленту
  void fillAll(Color c);              // всё игровое поле
  void addAll(Color c);

  // ---- логическая линия (как раньше) ----
  void set(int logical, Color c);
  void add(int logical, Color c);
  void addAA(float pos, Color c);     // субпиксельная точка со сглаживанием
  void fill(int from, int to, Color c);
  Color get(int logical) const;

  // ---- физические светодиоды ----
  void  setP(int physical, Color c);
  void  addP(int physical, Color c);
  Color getP(int physical) const;

  // ---- пути и маршруты ----
  void set(const Path& p, int i, Color c)      { setP(p.phys(i), c); }
  void add(const Path& p, int i, Color c)      { addP(p.phys(i), c); }
  void addAA(const Path& p, float pos, Color c);
  void fill(const Path& p, Color c);
  void fade(const Path& p, uint8_t keep);
  void addAA(const Route& r, float pos, Color c);
  void fill(const Route& r, Color c);

  // ---- сетка ----
  static int gridPhys(int col, int row) {
    if (col < 0 || col >= LED_GRID_COLS || row < 0 || row >= LED_GRID_ROWS) return -1;
    uint8_t p = LED_GRID[row][col];
    return p == LED_NONE ? -1 : p;
  }
  void  setXY(int col, int row, Color c)  { setP(gridPhys(col, row), c); }
  void  addXY(int col, int row, Color c)  { addP(gridPhys(col, row), c); }
  Color getXY(int col, int row) const      { return getP(gridPhys(col, row)); }
  void  addAAXY(float x, float y, Color c);        // билинейная точка на сетке
  void  fillRect(int col, int row, int w, int h, Color c);

  // ---- карта и режим ----
  uint8_t physicalOf(int logical) const;        // логический -> физический
  bool    isWall(int physical) const;           // физический вне логической линии?
  void    setWall(Color c);                     // перекрасить стены
  Color   wall() const { return _wall; }
  void    resetWall();                          // вернуть цвет из led_map.h
  // Полный холст: стен нет, игре принадлежат все физические светодиоды.
  void    setFullCanvas(bool on);
  bool    fullCanvas() const { return _full; }

  // Совместимость со старым кодом
  Color rgb(uint8_t r, uint8_t g, uint8_t b) const { return ::rgb(r, g, b); }
  Color hsv(uint8_t h, uint8_t s = 255, uint8_t v = 255) const { return colHsv(h, s, v); }

  // ---- диагностика ----
  void colorTest();          // R/G/B/W по всей ФИЗИЧЕСКОЙ ленте (проверка железа)
  void physicalScanTest();   // бегущий пиксель по ФИЗИЧЕСКИМ номерам (сбор карты)
  void oneByOneTest();       // бегущий пиксель по ЛОГИЧЕСКИМ номерам (проверка карты)

private:
  static const uint8_t NO_LOGICAL = 0xFF;

  void paintWalls();                      // залить стены их цветом (обычный режим)
  void rawPixel(int physical, Color c);   // напрямую в ленту, мимо карты
  void rawFill(Color c);

  Color   _buf[LED_PHYSICAL_COUNT];            // физический кадр
  Color   _wall = 0;                           // цвет стен
  bool    _full = false;                       // полный холст (без стен)
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
  // Поставить кадр в очередь на вывод (не чаще OLED_MIN_MS). Сама передача
  // по I2C идёт кусками в pump(), чтобы не замораживать опрос кнопок:
  // целый кадр 1 КиБ на 400 кГц — это ~25 мс, за которые можно
  // прозевать короткое нажатие.
  void flush();
  // Один кусок передачи (~3 мс). Звать в каждом проходе loop().
  void pump();
  // Вывод немедленно и целиком (заставки, экраны диагностики).
  void flushNow();
  bool busy() const { return _txLeft > 0; }

  void text(int x, int y, const char* s, uint8_t size = 1);
  void textCentered(int y, const char* s, uint8_t size = 1);
  // Центрирование внутри произвольной колонки (для сплит-экрана).
  void textCenteredIn(int x0, int x1, int y, const char* s, uint8_t size = 1);
  static int textWidth(const char* s, uint8_t size = 1);

  bool ok() const { return _ok; }

private:
  static const uint16_t FRAME_BYTES = OLED_WIDTH * OLED_HEIGHT / 8;
  static const uint8_t  CHUNK = 127;        // байт данных в одной I2C-транзакции

  Adafruit_SSD1306 _oled{OLED_WIDTH, OLED_HEIGHT, &Wire, -1};
  uint32_t _lastFlush = 0;
  bool     _ok = false;
  uint8_t  _addr = OLED_ADDR;

  // Копия кадра на время передачи: игра тем временем рисует следующий.
  uint8_t  _tx[FRAME_BYTES];
  uint16_t _txPos = 0;
  uint16_t _txLeft = 0;
  bool     _txHeaderSent = false;
};

// ============================================================
//  Звук (бузер) + вибрация
// ============================================================
struct Note {
  uint16_t freq;   // Гц, 0 = пауза
  uint16_t durMs;
};

// Мотив для фонового канала: заставка игры, играет один раз при старте
// (или по кругу, если попросить). Длительности нот — при темпе 1.0;
// Output::musicTempo() растягивает или сжимает их на лету.
struct MusicTrack {
  const Note* notes;
  uint8_t     count;
};
template <size_t N>
constexpr MusicTrack makeTrack(const Note (&n)[N]) { return MusicTrack{ n, (uint8_t)N }; }

// Переопределение одного эффекта: либо мелодия (notes != nullptr),
// либо слайд частоты (slideMs != 0). Вибрация у эффекта остаётся общей.
enum class Sfx : uint8_t;
struct SfxOverride {
  Sfx         id;
  const Note* notes;
  uint8_t     count;
  uint16_t    slideFrom, slideTo, slideMs;
};
// Банк звуков игры: список переопределений. Чего в банке нет —
// звучит по умолчанию. См. include/sound.h.
struct SfxBank {
  const SfxOverride* items;
  uint8_t            count;
};
template <size_t N>
constexpr SfxBank makeBank(const SfxOverride (&a)[N]) { return SfxBank{ a, (uint8_t)N }; }
#define SFX_MEL(ID, ARR)           { Sfx::ID, ARR, (uint8_t)(sizeof(ARR) / sizeof(ARR[0])), 0, 0, 0 }
#define SFX_SLIDE(ID, FROM, TO, MS) { Sfx::ID, nullptr, 0, FROM, TO, MS }

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
  void silence();                      // оборвать эффект (музыку не трогает)
  // Банк звуков текущей игры: переопределяет часть эффектов sfx().
  // nullptr — стандартный набор.
  void setBank(const SfxBank* b) { _bank = b; }

  // ---- фоновая музыка: второй логический канал на том же бузере ----
  // Мотив играет (один раз или по кругу), пока не идёт эффект:
  // tone/slide/melody/sfx перебивают его, а по окончании он продолжается
  // с той ноты, которая должна звучать по времени (позиция идёт и во
  // время эффекта).
  void music(const MusicTrack* track, bool loop = false);  // nullptr = остановить
  void musicTempo(float mul);           // 1.0 = как написано, 2.0 = вдвое быстрее
  void musicStop() { music(nullptr); }
  const MusicTrack* musicNow() const { return _track; }

  void buzz(uint16_t durMs);           // одиночный импульс вибрации
  void buzzPattern(uint8_t pulses, uint16_t onMs, uint16_t offMs);

  // Совместимость
  void beep()  { sfx(Sfx::Beep); }
  void click() { sfx(Sfx::Click); }
  void winSound()  { sfx(Sfx::Win); }
  void loseSound() { sfx(Sfx::Lose); }
  void startupFanfare() { sfx(Sfx::Fanfare); }

private:
  void writeTone(uint16_t freq, uint8_t duty = BUZZER_DUTY);
  bool effectActive() const { return _sliding || _melody || _toneEnd; }
  void musicUpdate(uint32_t now);

  uint32_t _toneEnd = 0;
  const Note* _melody = nullptr;
  uint8_t _melCount = 0;
  uint8_t _melIdx = 0;

  // Плавный слайд частоты
  bool     _sliding = false;
  uint32_t _slideStart = 0, _slideDur = 0;
  uint16_t _slideFrom = 0, _slideTo = 0;

  const SfxBank* _bank = nullptr;

  // Музыка
  const MusicTrack* _track = nullptr;
  bool     _musicLoop = false;
  float    _musicPos = 0;        // мс от начала темы (в "нотных" мс, до темпа)
  float    _musicTempo = 1.0f;
  uint32_t _musicLast = 0;       // millis() прошлого шага
  int      _musicNote = -1;      // какая нота звучит сейчас (-1 = ничего/перебита)
  bool     _musicGap = false;    // сейчас пауза между нотами

  // Вибро
  uint32_t _vibroNext = 0;
  uint8_t  _vibroLeft = 0;
  uint16_t _vibroOn = 0, _vibroOff = 0;
  bool     _vibroState = false;
};

// ============================================================
//  Кнопка (замыкает на GND, внутренняя подтяжка INPUT_PULLUP)
// ============================================================
//  Фронты ловятся ПРЕРЫВАНИЕМ: ISR только кладёт событие (нога,
//  уровень, millis) в общую очередь и ничего больше не делает.
//  update() разбирает накопившиеся события по меткам времени и гонит
//  по ним антидребезг — поэтому короткое нажатие не теряется, даже если
//  loop() в этот момент стоял (кусок кадра OLED, тайм-аут I2C), а время
//  фронта (pressedAt) точное, а не "когда цикл добежал".
//  Если событие потерялось (очередь переполнена), update() сверяет
//  последний известный уровень с ногой и достраивает фронт сам.
// ============================================================
class Button {
public:
  void begin(uint8_t pin);
  void update();

  bool pressed()  const { return _pressed; }    // фронт "нажали" (один вызов update)
  bool released() const { return _released; }   // фронт "отпустили"
  bool held()     const { return _raw; }        // удерживается прямо сейчас
  bool rawLow()   const { return _lastDebounceRaw; } // сырой уровень на ноге (без антидребезга)
  uint32_t heldMs() const;                      // сколько удерживается, мс
  uint32_t pressedAt() const { return _pressStart; } // millis фронта текущего/последнего нажатия
  uint32_t lastHeldMs() const { return _lastHeld; } // длительность последнего нажатия
  // Сколько раз контакт размыкался во время засчитанного удержания
  // (дребезг/чаттер микрика). Для теста кнопок.
  uint32_t holdGlitches() const { return _glitches; }
  void     resetStats() { _glitches = 0; }
  bool longPress();                             // один раз после BTN_LONG_MS
  // Автоповтор при удержании — для навигации по меню. pressedNow — фронт
  // нажатия ЭТОГО кадра (защёлкнутый в main), иначе повтор не стартует.
  bool repeat(bool pressedNow, uint16_t firstMs = 450, uint16_t rateMs = 130);

  uint8_t pin() const { return _pin; }

  // Сколько событий очередь не вместила (диагностика; в норме 0).
  static uint32_t droppedEvents();

private:
  static const uint8_t MAX_BUTTONS = 4;
  static Button* s_all[MAX_BUTTONS];
  static uint8_t s_count;

  static void isr(void* arg);                   // IRAM, только кладёт событие
  static void drain();                          // раздать события кнопкам
  void feed(bool low, uint32_t ms);             // фронт с ноги (из очереди или сверки)
  void settle(uint32_t now);                    // антидребезг и флаги

  uint8_t  _pin = 0;
  bool     _raw = false;
  bool     _lastDebounceRaw = false;
  bool     _pressed = false;
  bool     _released = false;
  bool     _longFired = false;
  bool     _longFlag = false;
  bool     _stuckWarned = false;
  uint32_t _glitches = 0;
  uint32_t _lastDebounceTime = 0;
  uint32_t _pressStart = 0;
  uint32_t _lastHeld = 0;
  uint32_t _nextRepeat = 0;
};

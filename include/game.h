#pragma once
#include <Arduino.h>
#include "config.h"
#include "hardware.h"
#include "fx.h"
#include "ui.h"

// ============================================================
//  Ввод: снимок состояния кнопок на текущий кадр
// ============================================================
struct BtnState {
  bool     pressed  = false;   // фронт "нажали"
  bool     released = false;   // фронт "отпустили"
  bool     held     = false;   // удерживается прямо сейчас
  uint32_t heldMs   = 0;       // сколько удерживается
  uint32_t lastHeldMs = 0;     // длительность последнего завершённого нажатия
};

struct Inputs {
  BtnState p1;    // зелёная кнопка, левый край ленты
  BtnState p2;    // синяя кнопка, правый край ленты
  BtnState menu;
};

// ============================================================
//  Контекст кадра: всё железо + время в одном месте
// ============================================================
struct Ctx {
  uint32_t   now;     // millis() на начало кадра
  uint32_t   dtMs;    // сколько прошло с прошлого кадра
  float      dt;      // то же в секундах
  LedStrip&  leds;
  Display&   dpy;
  Output&    out;
  Fx&        fx;
};

// ============================================================
//  Текст экрана "как играть" (показывается перед стартом игры).
//  Каждая строка должна помещаться в ширину OLED при size=1
//  (до ~21 символа) и состоять из ЗАГЛАВНЫХ букв — кириллический
//  шрифт не знает строчных букв и "Ё" (см. font_ru.h).
// ============================================================
struct RuleText {
  const char* const* lines = nullptr;
  uint8_t count = 0;

  RuleText() {}
  RuleText(const char* const* l, uint8_t c) : lines(l), count(c) {}
};

// ============================================================
//  Базовый класс игры
// ============================================================
class Game {
public:
  virtual ~Game() {}

  virtual const char* name() const = 0;        // как показывать в меню
  virtual const char* hint() const { return ""; }  // подсказка в подвале
  virtual uint8_t players() const { return 2; }    // 1 или 2
  // Кооператив: играют вдвоём, но против консоли и с общим счётом.
  // В меню такие игры помечаются "КО" вместо "1И"/"2И".
  virtual bool    coop() const { return false; }
  virtual Color   theme() const { return rgb(180, 180, 180); }  // цвет в меню
  virtual RuleText rules() const { return RuleText(); }  // экран "как играть"

  virtual void reset() = 0;
  virtual void update(const Inputs& in, Ctx& c) = 0;

  // Игра может попросить выйти в меню (например, после GAME OVER).
  virtual bool wantsExit() const { return false; }
};

// ============================================================
//  Матч "до N побед" — общая механика для игр на двоих
// ============================================================
enum class MatchPhase : uint8_t {
  Playing,        // идёт раунд
  RoundOver,      // показываем победителя раунда
  MatchOver       // показываем победителя матча
};

struct Match {
  int score[2] = {0, 0};
  int round = 1;
  int winner = 0;                       // 1 или 2 — кто выиграл раунд/матч
  MatchPhase phase = MatchPhase::Playing;
  uint32_t until = 0;

  void reset();
  // Зафиксировать победу игрока p (1 или 2). Сама решает, конец матча или нет.
  void winRound(int p, uint32_t now, Ctx& c);
  // Вызывать каждый кадр. Возвращает true, когда пора начинать новый раунд
  // (для MatchOver вместо этого сбрасывает матч и тоже возвращает true).
  bool tick(uint32_t now);

  bool playing() const { return phase == MatchPhase::Playing; }
  bool over() const { return phase == MatchPhase::MatchOver; }
};

// Общая отрисовка "празднования" на ленте: заливка цветом победителя,
// бегущие искры, для победы в матче — фейерверк.
void drawCelebration(Ctx& c, int winner, bool matchOver);

// Общая отрисовка результата на OLED для игр на двоих.
void drawMatchOled(Ctx& c, const char* title, const Match& m,
                   const Panel& left, const Panel& right, const char* foot);

// ============================================================
//  Рекорды в энергонезависимой памяти (NVS)
// ============================================================
class Store {
public:
  void begin();
  int  best(const char* key);
  // Возвращает true, если рекорд обновлён.
  bool submit(const char* key, int value);
  void wipe();
};

extern Store store;

// Маленький помощник: "1 игрок"/"2 игрока" в подвал OLED.
const char* playersLabel(uint8_t n);

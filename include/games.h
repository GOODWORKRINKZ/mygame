#pragma once
#include <Arduino.h>
#include "config.h"
#include "hardware.h"

// Снимок состояния игровых кнопок за кадр.
struct Inputs {
  bool p1Pressed = false;
  bool p1Released = false;
  bool p1Held = false;
  bool p2Pressed = false;
  bool p2Released = false;
  bool p2Held = false;
};

class Game {
public:
  virtual ~Game() {}
  virtual const char* name() const = 0;
  virtual void reset() = 0;
  virtual void update(uint32_t dtMs, const Inputs& in,
                      LedStrip& leds, Display& dpy, Output& out) = 0;
};

// ============================================================
//  1) "Перетягивание" (змейка): кто вытолкнет соперника из средней
//     зоны [8..23] на 16 светодиодах, тот выигрывает раунд.
// ============================================================
class TugGame : public Game {
public:
  const char* name() const override { return "TUG OF WAR"; }
  void reset() override;
  void update(uint32_t dtMs, const Inputs& in,
              LedStrip& leds, Display& dpy, Output& out) override;
private:
  int marker = LED_COUNT / 2;
  int score[2] = {0, 0};
  int round = 1;
  int winFlash = 0;        // 0 нет, 1/2 победитель раунда
  int over = 0;            // 0 нет, 1/2 победитель матча
  uint32_t flashUntil = 0;
  void render(LedStrip& leds);
  void drawOLED(Display& dpy);
};

// ============================================================
//  2) "Стрелялка": у каждого база с ХП, кнопка запускает снаряд,
//     свои снаряды взаимно уничтожаются при встрече.
// ============================================================
class ShooterGame : public Game {
public:
  const char* name() const override { return "SHOOTER"; }
  void reset() override;
  void update(uint32_t dtMs, const Inputs& in,
              LedStrip& leds, Display& dpy, Output& out) override;
private:
  int hp[2] = {SHOOTER_HP, SHOOTER_HP};
  int score[2] = {0, 0};
  int round = 1;
  int over = 0;
  int p1x = -1, p2x = -1;   // позиции снарядов (-1 = нет)
  uint32_t cd[2] = {0, 0};  // перезарядка
  uint32_t lastMove = 0;
  bool explode = false;
  int explodeAt = 0;
  uint32_t explodeUntil = 0;
  uint32_t overUntil = 0;
  bool _fxHit = false;       // запросить звук попадания в следующем update()
  bool _fxWin = false;       // запросить мелодию победы
  void hitBase(int attacker);
  void hitFx()   { _fxHit = true; }
  void roundWin(int p);
  void winFx()   { _fxWin = true; }
  void render(LedStrip& leds);
  void drawOLED(Display& dpy);
};

// ============================================================
//  3) "Пинг-понг": шпингалет копит энергию, при отпускании бьёт
//     на N пикселей. Мяч отбивается только во время удара; если
//     мяч залетает на обратном ходе — гол.
// ============================================================
enum class PaddlePhase : uint8_t { Idle, Charging, Strike, Return };

struct Paddle {
  PaddlePhase phase = PaddlePhase::Idle;
  int charge = 0;   // накопленная энергия (0..PONG_PADDLE_MAX)
  int ext = 0;      // текущий вылет шпингалета
  void tick(bool held);
  bool blocking() const { return phase == PaddlePhase::Strike && ext > 0; }
};

class PongGame : public Game {
public:
  const char* name() const override { return "PONG"; }
  void reset() override;
  void update(uint32_t dtMs, const Inputs& in,
              LedStrip& leds, Display& dpy, Output& out) override;
private:
  Paddle p1, p2;
  int ball = LED_COUNT / 2;
  int dir = 1;
  int score[2] = {0, 0};
  int round = 1;
  int over = 0;
  uint32_t lastTick = 0;
  uint32_t overUntil = 0;
  bool _fxWin = false;       // запросить звук победы
  bool _fxLose = false;      // запросить звук проигрыша
  void goal(int scorer);
  void queueWin()  { _fxWin  = true; }
  void queueLose() { _fxLose = true; }
  void render(LedStrip& leds);
  void drawOLED(Display& dpy);
};

// ============================================================
//  Менеджер игр (переключение маленькой кнопкой)
// ============================================================
class GameManager {
public:
  GameManager();
  Game* now();
  void next();
  void resetCurrent();
private:
  TugGame _tug;
  ShooterGame _shooter;
  PongGame _pong;
  Game* _games[3];
  int _cur = 0;
};

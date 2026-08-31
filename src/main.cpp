#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "hardware.h"
#include "games.h"

LedStrip   leds;
Display    dpy;
Output     out;
Button     btnP1, btnP2, btnMenu;
GameManager manager;

static void playBootAnimation();

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  leds.begin();
  dpy.begin();
  out.begin();
  btnP1.begin(PIN_BTN_P1);
  btnP2.begin(PIN_BTN_P2);
  btnMenu.begin(PIN_BTN_MENU);

  manager.resetCurrent();
  playBootAnimation();
}

// ============================================================
//  Заставка при включении: бегущая радуга + проверка LUT +
//  прогресс-бар на OLED + короткий "фанфары" бузером/вибрацией.
// ============================================================
static void playBootAnimation() {
  // 1) Звуковое приветствие (неблокирующе — крутится в loop через out.update).
  out.startupFanfare();

  // 2) Анимация на ленте: бегущая "комета" + хвост-радуга (~1.6 сек).
  const uint32_t STEP_MS = 22;
  const uint8_t  LEN = LED_COUNT;
  for (int pass = 0; pass < 3; pass++) {
    for (int i = -8; i < (int)LEN; i++) {
      leds.clear();
      for (int k = 0; k < 8; k++) {
        int p = i - k;
        if (p < 0 || p >= LEN) continue;
        // хвост плавно затухает, голова — белая
        uint8_t v = (uint8_t)(40 + (215 * (8 - k)) / 8);
        leds.setLogical(p, leds.hsv((uint16_t)(i * 14 + k * 30), 255, v));
      }
      leds.show();
      dpy.bootFrame(0);
      delay(STEP_MS);
    }
  }

  // 3) "Все онлайн" — заливка ленты цветом команды + финальный кадр OLED.
  leds.fillAll(leds.rgb(0, 120, 0));
  leds.show();
  dpy.bootFrame(1);
  delay(700);

  // 4) Список игр на OLED и переход к первой.
  leds.fillAll(0);
  leds.show();
  dpy.bootFrame(2);
  delay(900);

  dpy.splash(manager.now()->name(), "MENU: switch game");
}

void loop() {
  btnP1.update();
  btnP2.update();
  btnMenu.update();

  // Маленькая кнопка: короткое нажатие — следующая игра,
  // долгое нажатие — сброс текущей игры.
  if (btnMenu.longPress()) {
    manager.resetCurrent();
    dpy.splash(manager.now()->name(), "reset!");
  } else if (btnMenu.pressed()) {
    manager.next();
    manager.resetCurrent();
    dpy.splash(manager.now()->name(), "MENU: switch game");
  }

  Inputs in;
  in.p1Pressed  = btnP1.pressed();
  in.p1Released = btnP1.released();
  in.p1Held     = btnP1.held();
  in.p2Pressed  = btnP2.pressed();
  in.p2Released = btnP2.released();
  in.p2Held     = btnP2.held();

  manager.now()->update(0, in, leds, dpy, out);
  out.update();
  leds.show();
}

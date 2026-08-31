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
  dpy.splash(manager.now()->name(), "MENU: switch game");

  // стартовая "змейка" по ленте — проверка LUT и порядка светодиодов
  for (int i = 0; i < LED_COUNT; i++) {
    leds.clear();
    leds.setLogical(i, leds.rgb(20, 20, 40));
    leds.show();
    delay(12);
  }
  leds.clear();
  leds.show();
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

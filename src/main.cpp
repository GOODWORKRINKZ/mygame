#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "hardware.h"
#include "games.h"
#include "log.h"

LedStrip   leds;
Display    dpy;
Output     out;
Button     btnP1, btnP2, btnMenu;
GameManager manager;

static void playBootAnimation();

// Глобальное имя текущей игры — для heartbeat и для лога кнопок.
static const char* g_currentGame = "?";

void setup() {
  // Serial уже инициализирован platformio/Arduino, но на USB-CDC
  // первый printf иногда теряется — даём шине 200 мс подняться.
  Serial.begin(115200);
  delay(200);

  LOG_I("BOOT", "===== LED ARCADE BOOT =====");
  LOG_I("BOOT", "ESP32-C3, CPU %u MHz, flash %u, free heap %u",
         ESP.getCpuFreqMHz(), ESP.getFlashChipSize(), ESP.getFreeHeap());
  LOG_I("BOOT", "pinout: LED=%d OLED SDA/SCL=%d/%d BTN=%d/%d/%d BUZZ=%d VIBRO=%d",
         PIN_LED, PIN_OLED_SDA, PIN_OLED_SCL,
         PIN_BTN_P1, PIN_BTN_P2, PIN_BTN_MENU, PIN_BUZZER, PIN_VIBRO);

  randomSeed(esp_random());

  LOG_I("INIT", "leds.begin()...");
  leds.begin();
  LOG_I("INIT", "leds LUT built: %d logical -> physical", LED_COUNT);

  LOG_I("INIT", "dpy.begin() (OLED 128x64 I2C 0x%02X)...", OLED_ADDR);
  dpy.begin();
  LOG_I("INIT", "dpy OK");

  LOG_I("INIT", "out.begin() (buzzer pin=%d, vibro pin=%d)...", PIN_BUZZER, PIN_VIBRO);
  out.begin();

  LOG_I("INIT", "buttons.begin()...");
  btnP1.begin(PIN_BTN_P1);
  btnP2.begin(PIN_BTN_P2);
  btnMenu.begin(PIN_BTN_MENU);
  LOG_I("BTN", "raw levels P1=%d P2=%d MENU=%d (1=released, 0=pressed)",
         digitalRead(PIN_BTN_P1), digitalRead(PIN_BTN_P2), digitalRead(PIN_BTN_MENU));

  manager.resetCurrent();
  g_currentGame = manager.now()->name();
  LOG_I("BOOT", "starting game: %s", g_currentGame);

  playBootAnimation();
  LOG_I("BOOT", "boot animation done, entering main loop");
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
    g_currentGame = manager.now()->name();
    LOG_I("MENU", "RESET -> %s", g_currentGame);
    dpy.splash(g_currentGame, "reset!");
  } else if (btnMenu.pressed()) {
    manager.next();
    manager.resetCurrent();
    g_currentGame = manager.now()->name();
    LOG_I("MENU", "switch -> %s", g_currentGame);
    dpy.splash(g_currentGame, "MENU: switch game");
  }

  // Лог нажатий игровых кнопок — только фронты, чтобы не засорять.
  if (btnP1.pressed()) LOG_D("BTN", "P1 press");
  if (btnP2.pressed()) LOG_D("BTN", "P2 press");
  if (btnMenu.pressed()) LOG_V("BTN", "MENU press (handled above)");

  // Heartbeat: каждые 5 секунд показываем, что живы и какая игра активна.
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat >= 5000) {
    lastBeat = millis();
    LOG_I("HB", "uptime=%lus game=%s heap=%u",
          (unsigned long)(millis() / 1000), g_currentGame, ESP.getFreeHeap());
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

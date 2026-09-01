#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "hardware.h"
#include "fx.h"
#include "ui.h"
#include "game.h"
#include "games.h"
#include "log.h"

// ============================================================
//  LED ARCADE — точка входа
// ============================================================
//  Устройство живёт в трёх состояниях:
//    Boot — заставка при включении;
//    Menu — выбор игры (зелёная/синяя листают, MENU запускает);
//    Play — идёт игра (MENU выходит, удержание MENU перезапускает).
//
//  Кадр фиксированный (FRAME_MS), но кнопки опрашиваются в каждом
//  проходе loop() и фронты "защёлкиваются" — иначе быстрые нажатия
//  между кадрами терялись бы.

LedStrip    leds;
Display     dpy;
Output      out;
Fx          fx;
Button      btnP1, btnP2, btnMenu;
GameManager manager;

enum class AppState : uint8_t { Boot, Menu, Play };
static AppState state = AppState::Boot;

static uint32_t lastFrame = 0;
static uint32_t lastInput = 0;      // для "заставки" в простое
static bool     menuLongUsed = false;

// ---- защёлки фронтов между кадрами ----
struct Latch {
  bool     pressed = false;
  bool     released = false;
  uint32_t lastHeld = 0;
};
static Latch latchP1, latchP2, latchMenu;

static void pollButton(Button& b, Latch& l) {
  b.update();
  if (b.pressed())  l.pressed = true;
  if (b.released()) { l.released = true; l.lastHeld = b.lastHeldMs(); }
}

static void fillBtn(BtnState& s, Button& b, Latch& l) {
  s.pressed    = l.pressed;
  s.released   = l.released;
  s.held       = b.held();
  s.heldMs     = b.heldMs();
  s.lastHeldMs = l.lastHeld;
  l.pressed = l.released = false;
}

static void playBootAnimation();
static void enterMenu();
static void enterPlay();
static void updateMenu(const Inputs& in, Ctx& c);
static void runDiagnostics();

// ============================================================
//  setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  LOG_I("BOOT", "===== LED ARCADE =====");
  LOG_I("BOOT", "ESP32-C3 @%u MHz, flash %u, heap %u",
        ESP.getCpuFreqMHz(), ESP.getFlashChipSize(), ESP.getFreeHeap());
  LOG_I("BOOT", "pins: LED=%d I2C=%d/%d BTN=%d/%d/%d BUZZ=%d VIBRO=%d",
        PIN_LED, PIN_OLED_SDA, PIN_OLED_SCL,
        PIN_BTN_P1, PIN_BTN_P2, PIN_BTN_MENU, PIN_BUZZER, PIN_VIBRO);

  randomSeed(esp_random());

  leds.begin();
  dpy.begin();
  out.begin();
  store.begin();
  fx.reset();

  btnP1.begin(PIN_BTN_P1);
  btnP2.begin(PIN_BTN_P2);
  btnMenu.begin(PIN_BTN_MENU);
  LOG_I("BTN", "idle levels P1=%d P2=%d MENU=%d (1=released)",
        digitalRead(PIN_BTN_P1), digitalRead(PIN_BTN_P2), digitalRead(PIN_BTN_MENU));

#if DIAG_ON_BOOT
  runDiagnostics();
#endif

  playBootAnimation();
  enterMenu();
  lastFrame = millis();
  LOG_I("BOOT", "ready, %d games", manager.count());
}

// ============================================================
//  Заставка при включении
// ============================================================
static void playBootAnimation() {
  out.sfx(Sfx::Fanfare);

  // Комета с радужным хвостом пробегает ленту три раза.
  const uint8_t TAIL = 8;
  for (int pass = 0; pass < 3; pass++) {
    for (int i = -TAIL; i < LED_COUNT; i++) {
      leds.clear();
      for (int k = 0; k < TAIL; k++) {
        int p = i - k;
        if (p < 0 || p >= LED_COUNT) continue;
        uint8_t v = (uint8_t)(35 + (220 * (TAIL - k)) / TAIL);
        leds.set(p, colHsv((uint8_t)(i * 6 + k * 12), 255, v));
      }
      leds.show();
      out.update();
      uint8_t prog = (uint8_t)(((pass * LED_COUNT + i + TAIL) * 100) / (3 * LED_COUNT));
      Ui::bootFrame(dpy, 0, prog);
      delay(16);
    }
  }

  leds.fillAll(rgb(0, 90, 30));
  leds.show();
  Ui::bootFrame(dpy, 1, 100);
  delay(600);

  leds.clear();
  leds.show();
  Ui::bootFrame(dpy, 2, 100);
  delay(1400);
}

// ============================================================
//  Диагностика ленты (доступна из меню долгим нажатием MENU)
// ============================================================
static void runDiagnostics() {
  LOG_I("DIAG", "strip diagnostics start");
  dpy.clear();
  dpy.textCentered(20, "LED TEST", 2);
  dpy.textCentered(44, "R G B W + scan", 1);
  dpy.flushNow();
  leds.colorTest();
  leds.oneByOneTest();
  LOG_I("DIAG", "strip diagnostics done");
}

// ============================================================
//  Переходы между состояниями
// ============================================================
static void enterMenu() {
  state = AppState::Menu;
  fx.reset();
  out.silence();
  lastInput = millis();
  LOG_I("APP", "menu, cursor on %s", manager.now()->name());
}

static void enterPlay() {
  state = AppState::Play;
  fx.reset();
  manager.resetCurrent();
  LOG_I("APP", "start %s (%dP)", manager.now()->name(), manager.now()->players());
}

// ============================================================
//  Меню выбора игры
// ============================================================
static void updateMenu(const Inputs& in, Ctx& c) {
  Game* g = manager.now();

  // ---- лента: превью выбранной игры ----
  bool idle = (c.now - lastInput) > 15000;
  c.leds.clear();
  if (idle) {
    // режим "витрины" — просто красивая радуга
    Bg::rainbow(c.leds, c.now, 30, 70);
  } else {
    Color th = g->theme();
    Bg::comet(c.leds, c.now, th, 1400, 8);
    // сколько игроков: 1 или 2 ярких пикселя по краям
    c.leds.add(0, colScale(COL_P1, 160));
    if (g->players() == 2) c.leds.add(LED_COUNT - 1, colScale(COL_P2, 160));
  }

  // ---- OLED: список ----
  char foot[26];
  snprintf(foot, sizeof foot, "MENU=play  HOLD=test");
  Ui::menuFrame(c.dpy, "SELECT GAME", manager.names(), manager.playerCounts(),
                manager.count(), manager.index(), foot);
}

// ============================================================
//  loop
// ============================================================
void loop() {
  // Кнопки и звук обслуживаем на максимальной частоте: так не теряются
  // короткие нажатия и не рвутся мелодии.
  pollButton(btnP1, latchP1);
  pollButton(btnP2, latchP2);
  pollButton(btnMenu, latchMenu);
  out.update();

  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;

  uint32_t dtMs = now - lastFrame;
  if (dtMs > 100) dtMs = 100;          // после блокирующих пауз не «телепортируемся»
  lastFrame = now;

  Inputs in;
  fillBtn(in.p1, btnP1, latchP1);
  fillBtn(in.p2, btnP2, latchP2);
  fillBtn(in.menu, btnMenu, latchMenu);

  if (in.p1.pressed || in.p2.pressed || in.menu.pressed) lastInput = now;

  Ctx c{ now, dtMs, dtMs / 1000.0f, leds, dpy, out, fx };

  // ---- маленькая кнопка: короткое нажатие по отпусканию,
  //      длинное — сразу по достижении порога ----
  bool menuShort = false, menuLong = false;
  if (btnMenu.longPress()) { menuLong = true; menuLongUsed = true; }
  if (in.menu.released)    { menuShort = !menuLongUsed; menuLongUsed = false; }

  switch (state) {
    case AppState::Menu:
      if (in.p1.pressed || btnP1.repeat()) { manager.prev(); out.sfx(Sfx::Click); }
      if (in.p2.pressed || btnP2.repeat()) { manager.next(); out.sfx(Sfx::Click); }
      if (menuShort) { out.sfx(Sfx::Select); enterPlay(); break; }
      if (menuLong)  { runDiagnostics(); lastFrame = millis(); }
      updateMenu(in, c);
      break;

    case AppState::Play:
      if (menuShort) { out.sfx(Sfx::Back); enterMenu(); break; }
      if (menuLong)  { out.sfx(Sfx::Select); manager.resetCurrent(); fx.reset(); }
      manager.now()->update(in, c);
      break;

    default:
      enterMenu();
      break;
  }

  // Эффекты рисуются поверх кадра игры — так искры и вспышки
  // не мешают игровой логике и работают одинаково во всех играх.
  fx.update(c.dt);
  fx.draw(leds);
  leds.show();

  // Сердцебиение в лог: видно, что живы и где находимся.
  static uint32_t lastBeat = 0;
  if (now - lastBeat >= 10000) {
    lastBeat = now;
    LOG_I("HB", "up=%lus state=%s game=%s heap=%u",
          (unsigned long)(now / 1000),
          state == AppState::Menu ? "menu" : "play",
          manager.now()->name(), ESP.getFreeHeap());
  }
}

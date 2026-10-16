#include <Arduino.h>
#include "esp_system.h"
#include "config.h"
#include "hardware.h"
#include "fx.h"
#include "ui.h"
#include "game.h"
#include "games.h"
#include "sound.h"
#include "log.h"

// ============================================================
//  LED ARCADE — точка входа
// ============================================================
//  Устройство живёт в четырёх состояниях:
//    Boot — заставка при включении;
//    Menu — выбор игры (зелёная/синяя листают, MENU открывает "как играть");
//    Help — экран "как играть" (зелёная/синяя листают текст, MENU стартует
//           игру, удержание MENU — назад в меню);
//    Play — идёт игра (MENU выходит, удержание MENU перезапускает).
//
//  Кадр фиксированный (FRAME_MS). Фронты кнопок ловит прерывание и
//  складывает в очередь; каждый проход loop() разбирает её и
//  "защёлкивает" фронты до следующего кадра — так быстрые нажатия
//  между кадрами не теряются, даже если цикл где-то постоял. Кадр OLED
//  уходит по I2C кусками (Display::pump), а не одним 25-мс блоком.

LedStrip    leds;
Display     dpy;
Output      out;
Fx          fx;
Button      btnP1, btnP2, btnMenu;
GameManager manager;

enum class AppState : uint8_t { Boot, Menu, Help, Play };
static AppState state = AppState::Boot;

static uint32_t lastFrame = 0;
static uint32_t lastInput = 0;      // для "заставки" в простое
static bool     menuLongUsed = false;
static uint8_t  helpPage = 0;       // текущая страница экрана "как играть"

// ---- защёлки фронтов между кадрами ----
struct Latch {
  bool     pressed = false;
  bool     released = false;
  uint32_t lastHeld = 0;
  uint32_t pressedAt = 0;
};
static Latch latchP1, latchP2, latchMenu;

static void pollButton(Button& b, Latch& l) {
  b.update();
  if (b.pressed())  { l.pressed = true; l.pressedAt = b.pressedAt(); }
  if (b.released()) { l.released = true; l.lastHeld = b.lastHeldMs(); }
}

static void fillBtn(BtnState& s, Button& b, Latch& l) {
  s.pressed    = l.pressed;
  s.released   = l.released;
  s.held       = b.held();
  s.heldMs     = b.heldMs();
  s.lastHeldMs = l.lastHeld;
  s.pressedAt  = l.pressedAt;
  l.pressed = l.released = false;
}

static void playBootAnimation();
static void enterMenu();
static void enterHelp();
static void enterPlay();
static void previewLeds(Ctx& c, Game* g, bool allowIdle);
static void updateMenu(const Inputs& in, Ctx& c);
static void updateHelp(const Inputs& in, Ctx& c);
static void runDiagnostics();
static void runButtonTest();

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

  LOG_I("BOOT", "field: physical=%d logical=%d walls=%d middle=[%d..%d]",
        LED_PHYSICAL_COUNT, LED_COUNT, LED_PHYSICAL_COUNT - LED_COUNT,
        MIDDLE_LO, MIDDLE_HI);

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
  out.musicStop();
  char l1[26], l2[26];
  snprintf(l1, sizeof l1, "ФИЗ %d  ИГРА %d", LED_PHYSICAL_COUNT, LED_COUNT);
  snprintf(l2, sizeof l2, "СТЕНЫ %d", LED_PHYSICAL_COUNT - LED_COUNT);

  // 1) цвета по всей физической ленте — жива ли она вообще
  dpy.clear();
  dpy.textCentered(6, "ТЕСТ ЛЕНТЫ", 2);
  dpy.textCentered(30, "1/3 ЦВЕТА (ФИЗ)", 1);
  dpy.textCentered(44, l1, 1);
  dpy.flushNow();
  leds.colorTest();

  // 2) бегущий пиксель по физическим номерам — для сборки карты
  dpy.clear();
  dpy.textCentered(6, "ТЕСТ ЛЕНТЫ", 2);
  dpy.textCentered(30, "2/3 СКАН (ФИЗ)", 1);
  dpy.textCentered(44, "РОЗОВЫЙ = СТЕНА", 1);
  dpy.flushNow();
  leds.physicalScanTest();

  // 3) бегущий пиксель по логическим номерам — проверка карты
  dpy.clear();
  dpy.textCentered(6, "ТЕСТ ЛЕНТЫ", 2);
  dpy.textCentered(30, "3/3 СКАН (ЛОГ)", 1);
  dpy.textCentered(44, l2, 1);
  dpy.flushNow();
  leds.oneByOneTest();

  LOG_I("DIAG", "strip diagnostics done");

  // 4) живой тест кнопок — пока не отпустят удержанием MENU
  runButtonTest();
}

// ============================================================
//  Тест кнопок: сырой уровень на ноге, счётчик нажатий и длительность
//  последнего. Нужен, чтобы отличить железное залипание (уровень на
//  ноге остаётся LOW после отпускания) от программного.
//  Рукав зелёного горит, пока нога P1 читается LOW; рукав синего — P2;
//  середина — MENU. Выход — удержание MENU.
// ============================================================
static void runButtonTest() {
  LOG_I("DIAG", "button test: P1=GPIO%d P2=GPIO%d MENU=GPIO%d, hold MENU to exit",
        PIN_BTN_P1, PIN_BTN_P2, PIN_BTN_MENU);

  // мы попали сюда удержанием MENU — дождёмся, пока её отпустят
  while (digitalRead(PIN_BTN_MENU) == LOW) delay(5);
  btnMenu.update();
  btnMenu.longPress();

  Button* btn[3] = { &btnP1, &btnP2, &btnMenu };
  const char* nm[3] = { "ЗЕЛ", "СИН", "МЕНЮ" };
  for (int i = 0; i < 3; i++) btn[i]->resetStats();
  uint32_t cnt[3] = {0, 0, 0}, lastDur[3] = {0, 0, 0};
  uint32_t relAt[3] = {0, 0, 0};              // когда отпустили в последний раз
  uint32_t minDown = 9999, minUp = 9999;      // самое короткое нажатие / пауза (P1+P2)
  uint32_t lastDraw = 0;

  for (;;) {
    uint32_t t = millis();
    for (int i = 0; i < 3; i++) {
      btn[i]->update();
      if (btn[i]->pressed()) {
        cnt[i]++;
        // пауза между отпусканием и новым нажатием — это предел скорости долбежа
        if (i < 2 && relAt[i] && t - relAt[i] < minUp) minUp = t - relAt[i];
        LOG_D("BTN", "%s down (gap %lu ms)", nm[i], relAt[i] ? (unsigned long)(t - relAt[i]) : 0UL);
      }
      if (btn[i]->released()) {
        lastDur[i] = btn[i]->lastHeldMs();
        relAt[i] = t;
        if (i < 2 && lastDur[i] < minDown) minDown = lastDur[i];
        LOG_D("BTN", "%s up after %lu ms", nm[i], (unsigned long)lastDur[i]);
      }
    }
    if (btnMenu.longPress()) break;

    bool r1 = digitalRead(PIN_BTN_P1) == LOW;
    bool r2 = digitalRead(PIN_BTN_P2) == LOW;
    bool rm = digitalRead(PIN_BTN_MENU) == LOW;

    leds.setFullCanvas(true);
    leds.clear();
    leds.fill(ARM_P1, r1 ? COL_P1 : colScale(COL_P1, 18));
    leds.fill(ARM_P2, r2 ? COL_P2 : colScale(COL_P2, 18));
    leds.fill(CORE,   rm ? rgb(255, 255, 255) : rgb(12, 12, 12));
    // антидребезг согласен с ногой? если нет — кольцо подсвечивает рассинхрон
    bool mismatch = (r1 != btnP1.held()) || (r2 != btnP2.held());
    leds.fill(RING, mismatch ? rgb(120, 60, 0) : rgb(4, 4, 6));
    leds.show();

    uint32_t now = millis();
    if (now - lastDraw >= 100) {
      lastDraw = now;
      dpy.clear();
      dpy.textCentered(1, "ТЕСТ КНОПОК", 1);
      dpy.gfx().drawFastHLine(0, 15, OLED_WIDTH, SSD1306_WHITE);
      bool raw[3] = { r1, r2, rm };
      for (int i = 0; i < 3; i++) {
        char line[26];
        snprintf(line, sizeof line, "%-4s %s N%-3lu %4luМС", nm[i],
                 raw[i] ? "НАЖ" : "---", (unsigned long)cnt[i], (unsigned long)lastDur[i]);
        dpy.text(2, 18 + i * 9, line, 1);
      }
      char mm[26];
      uint32_t gl = btnP1.holdGlitches() + btnP2.holdGlitches();
      if (minDown == 9999) snprintf(mm, sizeof mm, "НАЖ = НОГА НА GND");
      else snprintf(mm, sizeof mm, "МИН %lu/%luМС РАЗРЫВ %lu",
                    (unsigned long)minDown, (unsigned long)(minUp == 9999 ? 0 : minUp),
                    (unsigned long)gl);
      dpy.textCentered(45, mismatch ? "ДРЕБЕЗГ/РАССИНХРОН" : mm, 1);
      dpy.textCentered(57, "УДЕРЖИ МЕНЮ = ВЫХОД", 1);
      dpy.flush();
    }
    dpy.pump();
    delay(1);
  }

  leds.setFullCanvas(false);
  leds.clear();
  leds.show();
  LOG_I("DIAG", "button test done: P1=%lu P2=%lu MENU=%lu presses, dropped IRQ events=%lu",
        (unsigned long)cnt[0], (unsigned long)cnt[1], (unsigned long)cnt[2],
        (unsigned long)Button::droppedEvents());
}

// ============================================================
//  Переходы между состояниями
// ============================================================
static void enterMenu() {
  state = AppState::Menu;
  fx.reset();
  out.silence();
  out.setBank(nullptr);
  out.music(&MUSIC_MENU);
  leds.setFullCanvas(false);
  lastInput = millis();
  LOG_I("APP", "menu, cursor on %s", manager.now()->name());
}

static void enterHelp() {
  state = AppState::Help;
  helpPage = 0;
  fx.reset();
  out.silence();
  // на экране правил звучит заставка выбранной игры — так её можно "послушать"
  out.setBank(manager.now()->sounds());
  out.music(manager.now()->music());
  leds.setFullCanvas(false);
  lastInput = millis();
  LOG_I("APP", "help for %s", manager.now()->name());
}

static void enterPlay() {
  state = AppState::Play;
  fx.reset();
  leds.resetWall();          // меню подкрашивает стены — возвращаем как было
  leds.setFullCanvas(manager.now()->fullCanvas());
  out.setBank(manager.now()->sounds());
  out.music(manager.now()->music());
  manager.resetCurrent();
  LOG_I("APP", "start %s (%dP)", manager.now()->name(), manager.now()->players());
}

// ============================================================
//  Лента: превью выбранной игры (общее для меню и экрана "как играть")
// ============================================================
static void previewLeds(Ctx& c, Game* g, bool allowIdle) {
  bool idle = allowIdle && (c.now - lastInput) > 15000;
  c.leds.clear();
  if (idle) {
    // режим "витрины" — радуга и по полю, и по стенам
    Bg::rainbow(c.leds, c.now, 30, 70);
    c.leds.setWall(colHsv((uint8_t)((c.now * 30) / 1000), 255, 40));
  } else {
    // стены дышат цветом выбранной игры — видно, что курсор двигается
    c.leds.setWall(colScale(g->theme(), (uint8_t)(14 + sin8((uint8_t)(c.now / 8)) / 8)));
    Color th = g->theme();
    Bg::comet(c.leds, c.now, th, 1400, 8);
    // сколько игроков: 1 или 2 ярких пикселя по краям
    c.leds.add(0, colScale(COL_P1, 160));
    if (g->players() == 2) c.leds.add(LED_COUNT - 1, colScale(COL_P2, 160));
  }
}

// ============================================================
//  Меню выбора игры
// ============================================================
static void updateMenu(const Inputs& in, Ctx& c) {
  Game* g = manager.now();
  previewLeds(c, g, true);

  // ---- OLED: список ----
  char foot[26];
  snprintf(foot, sizeof foot, "МЕНЮ=ИГРАТЬ  УДЕРЖ=ТЕСТ");
  Ui::menuFrame(c.dpy, "ВЫБОР ИГРЫ", manager.names(), manager.playerCounts(),
                manager.count(), manager.index(), foot);
}

// ============================================================
//  Экран "как играть" — описание игры перед стартом.
//  Листается кнопками P1/P2 (той же парой, что и в меню), MENU
//  запускает саму игру, долгое нажатие MENU — назад в меню.
// ============================================================
static const uint8_t HELP_LINES_PER_PAGE = 4;

static void updateHelp(const Inputs& in, Ctx& c) {
  Game* g = manager.now();
  previewLeds(c, g, false);

  RuleText rt = g->rules();
  uint8_t pageCount = 1;
  if (rt.count > 0) {
    pageCount = (rt.count + HELP_LINES_PER_PAGE - 1) / HELP_LINES_PER_PAGE;
    if (pageCount < 1) pageCount = 1;
  }
  if (helpPage >= pageCount) helpPage = pageCount - 1;

  if (btnP1.repeat(in.p1.pressed) && pageCount > 1) {
    helpPage = (uint8_t)((helpPage + 1) % pageCount);
    out.sfx(Sfx::Click);
  }
  if (btnP2.repeat(in.p2.pressed) && pageCount > 1) {
    helpPage = (uint8_t)(helpPage == 0 ? pageCount - 1 : helpPage - 1);
    out.sfx(Sfx::Click);
  }

  const char* const* pageLines = nullptr;
  uint8_t linesOnPage = 0;
  if (rt.count > 0) {
    uint8_t start = helpPage * HELP_LINES_PER_PAGE;
    linesOnPage = (uint8_t)((rt.count - start < HELP_LINES_PER_PAGE)
                             ? (rt.count - start) : HELP_LINES_PER_PAGE);
    pageLines = rt.lines + start;
  }

  char right[8];
  snprintf(right, sizeof right, "%d/%d", helpPage + 1, pageCount);
  Ui::rulesFrame(c.dpy, g->name(), right, pageLines, linesOnPage, "МЕНЮ=ИГРАТЬ  ЛИСТАЙ");
}

// ============================================================
//  loop
// ============================================================
void loop() {
  // Кнопки и звук обслуживаем на максимальной частоте: так не теряются
  // короткие нажатия и не рвутся мелодии.
#if BTN_STRESS_TEST
  delay(BTN_STRESS_MS);   // имитация тормозящего цикла: фронты всё равно ловит ISR
#endif
  pollButton(btnP1, latchP1);
  pollButton(btnP2, latchP2);
  pollButton(btnMenu, latchMenu);
  out.update();
  dpy.pump();          // кадр OLED уходит по I2C кусками, между опросами кнопок

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
      // Зелёная (физически справа) — листает ВНИЗ, синяя (слева) — ВВЕРХ.
      if (btnP1.repeat(in.p1.pressed)) { manager.next(); out.sfx(Sfx::Click); }
      if (btnP2.repeat(in.p2.pressed)) { manager.prev(); out.sfx(Sfx::Click); }
      if (menuShort) { out.sfx(Sfx::Select); enterHelp(); break; }
      if (menuLong)  { runDiagnostics(); lastFrame = millis(); out.music(&MUSIC_MENU); }
      updateMenu(in, c);
      break;

    case AppState::Help:
      // P1/P2 листают описание, MENU — старт игры, удержание — назад в меню.
      if (menuShort) { out.sfx(Sfx::Select); enterPlay(); break; }
      if (menuLong)  { out.sfx(Sfx::Back); enterMenu(); break; }
      updateHelp(in, c);
      break;

    case AppState::Play:
      if (menuShort) { out.sfx(Sfx::Back); enterMenu(); break; }
      if (menuLong)  { out.sfx(Sfx::Select); manager.resetCurrent(); fx.reset();
                       out.music(manager.now()->music()); }
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
          state == AppState::Menu ? "menu" : state == AppState::Help ? "help" : "play",
          manager.now()->name(), ESP.getFreeHeap());
  }
}

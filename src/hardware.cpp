#include "hardware.h"

// ============================== LedStrip ==============================

void LedStrip::begin() {
  map.build(PANELS, PANEL_COUNT);   // строим LUT из раскладки панелей
  _strip.begin();
  _strip.setBrightness(LED_BRIGHTNESS);
  _strip.clear();
  _strip.show();
  LOG_I("LED", "strip ready: pin=%d (PIN_LED=%d) count=%d type=WS2812 GRB @800kHz",
         ACTIVE_PIN, PIN_LED, LedMap::COUNT);
}

void LedStrip::clear() { _strip.clear(); }

void LedStrip::show() { _strip.show(); }

void LedStrip::setLogical(uint8_t l, uint8_t r, uint8_t g, uint8_t b) {
  if (l >= LedMap::COUNT) return;
  _strip.setPixelColor(map[l], r, g, b);
}

void LedStrip::setLogical(uint8_t l, uint32_t color) {
  if (l >= LedMap::COUNT) return;
  _strip.setPixelColor(map[l], color);
}

void LedStrip::fillLogical(uint8_t from, uint8_t to, uint32_t color) {
  for (uint8_t i = from; i <= to; i++) setLogical(i, color);
}

void LedStrip::fillAll(uint32_t color) {
  fillLogical(0, LedMap::COUNT - 1, color);
}

uint32_t LedStrip::hsv(uint16_t hue, uint8_t sat, uint8_t val) {
  uint8_t region = hue / 43;
  uint8_t rem = (hue - region * 43) * 6;
  uint8_t p = (val * (255 - sat)) >> 8;
  uint8_t q = (val * (255 - ((sat * rem) >> 8))) >> 8;
  uint8_t t = (val * (255 - ((sat * (255 - rem)) >> 8))) >> 8;
  uint8_t r, g, b;
  switch (region) {
    case 0: r = val; g = t;   b = p;   break;
    case 1: r = q;   g = val; b = p;   break;
    case 2: r = p;   g = val; b = t;   break;
    case 3: r = p;   g = q;   b = val; break;
    case 4: r = t;   g = p;   b = val; break;
    default:r = val; g = p;   b = q;   break;
  }
  return _strip.Color(r, g, b);
}

void LedStrip::colorTest() {
  // Включаем по очереди: красный, зелёный, синий, белый, выкл.
  // Если цвета перепутаны — увидишь по факту (R зажжётся зелёным и т.п.).
  const uint32_t colors[] = {
    rgb(255, 0, 0),    // RED
    rgb(0, 255, 0),    // GREEN
    rgb(0, 0, 255),    // BLUE
    rgb(255, 255, 255) // WHITE
  };
  const char* names[] = { "RED", "GREEN", "BLUE", "WHITE" };
  for (int c = 0; c < 4; c++) {
    LOG_I("LED", "color test: %s on all %d pixels", names[c], LedMap::COUNT);
    fillAll(colors[c]);
    show();
    delay(500);
  }
  fillAll(0);
  show();
  delay(200);
}

void LedStrip::oneByOneTest() {
  // Бежит "бегущий огонёк" по логическим индексам. Каждый пиксель горит
  // 150 мс, остальные погашены. Позволяет увидеть:
  //   - работают ли вообще все 32 пикселя ленты;
  //   - в каком порядке они физически расположены;
  //   - работает ли LUT (логический 0 -> какой физический).
  LOG_I("LED", "oneByOne: lighting logical pixels 0..%d sequentially", LedMap::COUNT - 1);
  for (uint8_t i = 0; i < LedMap::COUNT; i++) {
    clear();
    setLogical(i, rgb(255, 80, 0));   // яркий оранжевый — легко отличить от шума
    show();
    LOG_V("LED", "logical=%u -> physical=%u", i, (unsigned)map[i]);
    delay(120);
  }
  clear();
  show();
  LOG_I("LED", "oneByOne done");
}

// ============================== Display ==============================

void Display::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  // periphBegin=false: шина уже инициализирована на наших пинах выше
  bool ok = _oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false);
  if (!ok) {
    LOG_W("OLED", "addr 0x%02X not found, trying 0x3D", OLED_ADDR);
    ok = _oled.begin(SSD1306_SWITCHCAPVCC, 0x3D, true, false);
  }
  if (!ok) {
    LOG_E("OLED", "SSD1306 not found on I2C! check wiring to SDA=%d SCL=%d",
          PIN_OLED_SDA, PIN_OLED_SCL);
  } else {
    LOG_I("OLED", "SSD1306 OK at 0x%02X", OLED_ADDR);
  }
  _oled.clearDisplay();
  _oled.display();
  _oled.setTextColor(SSD1306_WHITE);
}

void Display::flush() {
  if (millis() - _lastFlush >= 80) {
    _lastFlush = millis();
    _oled.display();
  }
}

void Display::splash(const char* title, const char* sub) {
  _oled.clearDisplay();
  _oled.setTextSize(2);
  _oled.setCursor(0, 8);
  _oled.print(title);
  _oled.setTextSize(1);
  _oled.setCursor(0, 38);
  _oled.print(sub);
  _lastFlush = 0;
  _oled.display();
}

void Display::gameFrame(const char* title, int s1, int s2,
                        const char* l1, const char* l2) {
  _oled.clearDisplay();
  _oled.setTextSize(1);
  _oled.setCursor(0, 0);
  _oled.print(title);
  _oled.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);

  _oled.setTextSize(3);
  _oled.setCursor(8, 16);
  _oled.print(s1);
  _oled.setCursor(56, 16);
  _oled.print("-");
  _oled.setCursor(84, 16);
  _oled.print(s2);

  _oled.setTextSize(1);
  _oled.setCursor(0, 46);
  _oled.print(l1);
  _oled.setCursor(0, 56);
  _oled.print(l2);
  flush();
}

// Центрирует строку с учётом моноширинного шрифта Adafruit GFX:
// ширина символа = 6 пикселей при size=1, 12 при size=2 и т.д.
void Display::textCentered(int y, const char* s, uint8_t size) {
  _oled.setTextSize(size);
  uint16_t w = strlen(s) * 6 * size;
  int16_t x = (OLED_WIDTH - (int)w) / 2;
  if (x < 0) x = 0;
  _oled.setCursor(x, y);
  _oled.print(s);
}

void Display::bootFrame(uint8_t phase) {
  _oled.clearDisplay();

  if (phase == 0) {
    // "Загрузка": бегущая полоска внизу
    textCentered(8,  "LED ARCADE", 2);
    _oled.setTextSize(1);
    _oled.setCursor(0, 30);
    _oled.print("32-pixel console");
    _oled.drawRect(10, 48, OLED_WIDTH - 20, 12, SSD1306_WHITE);
    static uint8_t t = 0;
    uint8_t w = (t * (OLED_WIDTH - 24)) / 32;
    _oled.fillRect(12, 50, w, 8, SSD1306_WHITE);
    t = (t + 1) % 32;
  } else if (phase == 1) {
    // "Системы онлайн"
    _oled.setTextSize(2);
    textCentered(10, "READY!", 2);
    _oled.setTextSize(1);
    textCentered(36, "press MENU to start", 1);
    // декоративная "змейка" вокруг
    for (uint8_t i = 0; i < 8; i++) {
      int x = 8 + i * 14;
      _oled.fillCircle(x, 56, 4, SSD1306_WHITE);
    }
  } else {
    // список игр
    _oled.setTextSize(2);
    textCentered(2,  "GAMES", 2);
    _oled.setTextSize(1);
    _oled.setCursor(8, 22); _oled.print("> Tug of War");
    _oled.setCursor(8, 34); _oled.print("  Shooter");
    _oled.setCursor(8, 46); _oled.print("  Pong");
    _oled.setCursor(0, 58); _oled.print("MENU: next  HOLD: reset");
  }
  _oled.display();
}

// ============================== Output ==============================

#define BUZZER_CH 0

void Output::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  ledcSetup(BUZZER_CH, 1000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);
  LOG_I("BUZZ", "ledc ch=%d pin=%d", BUZZER_CH, PIN_BUZZER);

  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);
  LOG_I("VIBRO", "pin=%d", PIN_VIBRO);
}

void Output::update() {
  uint32_t now = millis();

  if (_melody) {
    if (now >= _toneEnd) {
      if (_melIdx >= _melCount) {
        _melody = nullptr;
        ledcWrite(BUZZER_CH, 0);
      } else {
        const Note& n = _melody[_melIdx++];
        ledcWriteTone(BUZZER_CH, n.freq);
        ledcWrite(BUZZER_CH, 128);
        _toneEnd = now + n.durMs;
      }
    }
  } else if (_toneEnd && now >= _toneEnd) {
    ledcWrite(BUZZER_CH, 0);
    _toneEnd = 0;
  }

  if (_vibroEnd && now >= _vibroEnd) {
    digitalWrite(PIN_VIBRO, LOW);
    _vibroEnd = 0;
  }
}

void Output::tone(uint16_t freq, uint16_t durMs) {
  _melody = nullptr;
  ledcWriteTone(BUZZER_CH, freq);
  ledcWrite(BUZZER_CH, 128);
  _toneEnd = millis() + durMs;
}

void Output::melody(const Note* notes, uint8_t count) {
  _melody = notes;
  _melCount = count;
  _melIdx = 0;
  _toneEnd = 0;
}

void Output::beep()  { tone(1319, 60); }
void Output::click() { tone(988, 30); }

void Output::buzz(uint16_t durMs) {
  digitalWrite(PIN_VIBRO, HIGH);
  _vibroEnd = millis() + durMs;
}

static const Note WIN_NOTES[]  = { {784, 80}, {988, 80}, {1319, 80}, {1568, 160} };
static const Note LOSE_NOTES[] = { {660, 80}, {440, 160} };

void Output::winSound()  { melody(WIN_NOTES, 4);  buzz(250); }
void Output::loseSound() { melody(LOSE_NOTES, 2); buzz(200); }

// "Пиу-пиу-пиу-вжуух" — короткое приветствие при старте.
static const Note STARTUP_NOTES[] = {
  { 880, 70 }, { 1175, 70 }, { 1568, 70 }, { 1976, 220 }
};

void Output::startupFanfare() {
  melody(STARTUP_NOTES, 4);
  buzz(350);
}

// ============================== Button ==============================

void Button::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT_PULLUP);
}

void Button::update() {
  bool nowRaw = digitalRead(_pin) == LOW;   // активный низкий
  uint32_t now = millis();

  if (nowRaw != _lastDebounceRaw) {
    _lastDebounceTime = now;
    _lastDebounceRaw = nowRaw;
  }
  bool stable = (now - _lastDebounceTime >= BTN_DEBOUNCE_MS) ? nowRaw : _raw;

  _pressed  = stable && !_raw;
  _released = !stable && _raw;

  if (_pressed) {
    _pressStart = now;
    _longFired = false;
    _longFlag = false;
  }

  _raw = stable;

  if (_raw && !_longFired && (now - _pressStart) >= BTN_LONG_MS) {
    _longFired = true;
    _longFlag = true;
  }
}

bool Button::longPress() {
  bool r = _longFlag;
  _longFlag = false;
  return r;
}

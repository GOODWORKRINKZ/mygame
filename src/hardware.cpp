#include "hardware.h"

// ============================== LedStrip ==============================

void LedStrip::begin() {
  map.build(PANELS, PANEL_COUNT);   // строим LUT из раскладки панелей
  _strip.begin();
  _strip.setBrightness(LED_BRIGHTNESS);
  _strip.clear();
  _strip.show();
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

// ============================== Display ==============================

void Display::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  // periphBegin=false: шина уже инициализирована на наших пинах выше
  if (!_oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false)) {
    _oled.begin(SSD1306_SWITCHCAPVCC, 0x3D, true, false);   // некоторые модули на 0x3D
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

// ============================== Output ==============================

#define BUZZER_CH 0

void Output::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  ledcSetup(BUZZER_CH, 1000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);

  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);
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

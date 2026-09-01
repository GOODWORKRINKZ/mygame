#include "hardware.h"
#include <math.h>

// ============================================================
//  Утилиты цвета
// ============================================================

Color colScale(Color c, uint8_t s) {
  uint16_t r = ((uint16_t)colR(c) * s) >> 8;
  uint16_t g = ((uint16_t)colG(c) * s) >> 8;
  uint16_t b = ((uint16_t)colB(c) * s) >> 8;
  return rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

Color colLerp(Color a, Color b, uint8_t t) {
  uint8_t r  = (uint8_t)(colR(a) + (((int)colR(b) - (int)colR(a)) * t) / 255);
  uint8_t g  = (uint8_t)(colG(a) + (((int)colG(b) - (int)colG(a)) * t) / 255);
  uint8_t bl = (uint8_t)(colB(a) + (((int)colB(b) - (int)colB(a)) * t) / 255);
  return rgb(r, g, bl);
}

Color colHsv(uint8_t hue, uint8_t sat, uint8_t val) {
  uint8_t region = hue / 43;
  uint8_t rem = (uint8_t)((hue - region * 43) * 6);
  uint8_t p = (uint8_t)(((uint16_t)val * (255 - sat)) >> 8);
  uint8_t q = (uint8_t)(((uint16_t)val * (255 - (((uint16_t)sat * rem) >> 8))) >> 8);
  uint8_t t = (uint8_t)(((uint16_t)val * (255 - (((uint16_t)sat * (255 - rem)) >> 8))) >> 8);
  switch (region) {
    case 0:  return rgb(val, t, p);
    case 1:  return rgb(q, val, p);
    case 2:  return rgb(p, val, t);
    case 3:  return rgb(p, q, val);
    case 4:  return rgb(t, p, val);
    default: return rgb(val, p, q);
  }
}

// Четверть синусоиды, 65 точек. Дешевле, чем звать sinf() десятки раз за кадр
// на ESP32-C3 (у RISC-V ядра нет аппаратной плавающей точки).
static const uint8_t SIN_Q[65] = {
    0,  6, 12, 19, 25, 31, 37, 44, 50, 56, 62, 68, 74, 80, 86, 92,
   98,104,109,115,121,126,132,137,142,147,152,157,162,167,172,177,
  181,185,190,194,198,202,206,209,213,216,219,222,225,228,231,233,
  236,238,240,242,243,245,247,248,249,250,251,252,253,253,254,254,
  255
};

uint8_t sin8(uint8_t theta) {
  uint8_t quad = theta >> 6;      // четверть периода 0..3
  uint8_t idx  = theta & 0x3F;    // положение внутри четверти 0..63
  uint8_t v;
  switch (quad) {
    case 0:  v = SIN_Q[idx];      return (uint8_t)(128 + v / 2);
    case 1:  v = SIN_Q[64 - idx]; return (uint8_t)(128 + v / 2);
    case 2:  v = SIN_Q[idx];      return (uint8_t)(127 - v / 2);
    default: v = SIN_Q[64 - idx]; return (uint8_t)(127 - v / 2);
  }
}

uint8_t tri8(uint8_t theta) {
  return theta < 128 ? (uint8_t)(theta * 2) : (uint8_t)((255 - theta) * 2);
}

// ============================================================
//  LedStrip
// ============================================================

static uint8_t GAMMA8[256];

static void buildGamma() {
  for (int i = 0; i < 256; i++) {
    float v = powf(i / 255.0f, 1.9f) * 255.0f + 0.5f;
    uint8_t g = (uint8_t)v;
    if (i > 0 && g == 0) g = 1;   // не гасим самые тусклые оттенки полностью
    GAMMA8[i] = g;
  }
}

void LedStrip::begin() {
  buildGamma();

  // Обратная карта: физический -> логический. Всё, что не встретилось
  // в LED_LOGICAL_MAP, остаётся NO_LOGICAL и станет "стеной".
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) _phys2log[p] = NO_LOGICAL;
  int walls = LED_PHYSICAL_COUNT;
  for (int l = 0; l < LED_COUNT; l++) {
    uint8_t p = LED_LOGICAL_MAP[l];
    if (p >= LED_PHYSICAL_COUNT) {
      LOG_E("LED", "map[%d] = %u выходит за LED_PHYSICAL_COUNT=%d", l, p, LED_PHYSICAL_COUNT);
      continue;
    }
    if (_phys2log[p] != NO_LOGICAL)
      LOG_W("LED", "физический %u указан в карте дважды (логические %u и %d)",
            p, _phys2log[p], l);
    else
      walls--;
    _phys2log[p] = (uint8_t)l;
  }

  resetWall();
  _strip.begin();
  _strip.setBrightness(LED_BRIGHTNESS);
  _strip.clear();
  _strip.show();
  clear();
  LOG_I("LED", "strip ready: pin=%d physical=%d logical=%d walls=%d gamma=%d",
        PIN_LED, LED_PHYSICAL_COUNT, LED_COUNT, walls, (int)LED_GAMMA);
}

void LedStrip::resetWall() { _wall = rgb(WALL_R, WALL_G, WALL_B); }

uint8_t LedStrip::physicalOf(int logical) const {
  return (logical >= 0 && logical < LED_COUNT) ? LED_LOGICAL_MAP[logical] : 0;
}

bool LedStrip::isWall(int physical) const {
  return (physical < 0 || physical >= LED_PHYSICAL_COUNT)
         || _phys2log[physical] == NO_LOGICAL;
}

void LedStrip::clear() {
  for (int i = 0; i < LED_COUNT; i++) _buf[i] = 0;
}

void LedStrip::fade(uint8_t keep) {
  for (int i = 0; i < LED_COUNT; i++) _buf[i] = colScale(_buf[i], keep);
}

void LedStrip::set(int l, Color c) {
  if (l < 0 || l >= LED_COUNT) return;
  _buf[l] = c;
}

void LedStrip::add(int l, Color c) {
  if (l < 0 || l >= LED_COUNT) return;
  uint16_t r = colR(_buf[l]) + colR(c);
  uint16_t g = colG(_buf[l]) + colG(c);
  uint16_t b = colB(_buf[l]) + colB(c);
  _buf[l] = rgb(r > 255 ? 255 : (uint8_t)r,
                g > 255 ? 255 : (uint8_t)g,
                b > 255 ? 255 : (uint8_t)b);
}

// Точка с дробной координатой: яркость делится между двумя соседними
// пикселями. Именно это превращает рывки мяча в плавное скольжение.
void LedStrip::addAA(float pos, Color c) {
  int i0 = (int)floorf(pos);
  float f = pos - (float)i0;
  uint8_t w1 = (uint8_t)(f * 255.0f);
  add(i0,     colScale(c, (uint8_t)(255 - w1)));
  add(i0 + 1, colScale(c, w1));
}

void LedStrip::fill(int from, int to, Color c) {
  if (from > to) { int t = from; from = to; to = t; }
  if (from < 0) from = 0;
  if (to > LED_COUNT - 1) to = LED_COUNT - 1;
  for (int i = from; i <= to; i++) _buf[i] = c;
}

void LedStrip::fillAll(Color c) { fill(0, LED_COUNT - 1, c); }

void LedStrip::addAll(Color c) {
  for (int i = 0; i < LED_COUNT; i++) add(i, c);
}

Color LedStrip::get(int l) const {
  return (l < 0 || l >= LED_COUNT) ? 0 : _buf[l];
}

void LedStrip::rawPixel(int physical, Color c) {
  if (physical < 0 || physical >= LED_PHYSICAL_COUNT) return;
#if LED_GAMMA
  _strip.setPixelColor(physical, GAMMA8[colR(c)], GAMMA8[colG(c)], GAMMA8[colB(c)]);
#else
  _strip.setPixelColor(physical, colR(c), colG(c), colB(c));
#endif
}

void LedStrip::rawFill(Color c) {
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) rawPixel(p, c);
}

void LedStrip::show() {
  // Один проход по физической ленте: игровые пиксели берём из кадра,
  // всё остальное заливаем цветом стен.
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) {
    uint8_t l = _phys2log[p];
    rawPixel(p, (l == NO_LOGICAL) ? _wall : _buf[l]);
  }
  _strip.show();
}

void LedStrip::colorTest() {
  // Проверка железа: гоняем цвета по ВСЕЙ физической ленте, мимо карты.
  const Color colors[] = { rgb(255,0,0), rgb(0,255,0), rgb(0,0,255), rgb(255,255,255) };
  const char* names[]  = { "RED", "GREEN", "BLUE", "WHITE" };
  for (int c = 0; c < 4; c++) {
    LOG_I("LED", "color test: %s on all %d physical pixels", names[c], LED_PHYSICAL_COUNT);
    rawFill(colors[c]);
    _strip.show();
    delay(450);
  }
  rawFill(0);
  _strip.show();
  delay(150);
}

void LedStrip::physicalScanTest() {
  // Бежим по ФИЗИЧЕСКИМ номерам: именно этот прогон нужен, чтобы
  // записать порядок светодиодов и собрать LED_LOGICAL_MAP.
  LOG_I("LED", "physical scan: 0..%d", LED_PHYSICAL_COUNT - 1);
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) {
    rawFill(0);
    // стены подсвечиваем своим цветом — сразу видно, какие выпадают из игры
    rawPixel(p, isWall(p) ? rgb(255, 0, 160) : rgb(255, 90, 0));
    _strip.show();
    LOG_V("LED", "physical=%d %s", p, isWall(p) ? "WALL" : "game");
    delay(110);
  }
  rawFill(0);
  _strip.show();
}

void LedStrip::oneByOneTest() {
  // Бежим по ЛОГИЧЕСКИМ номерам: огонёк должен идти ровно так,
  // как ты хочешь видеть игровое поле. Стены при этом горят своим цветом.
  LOG_I("LED", "logical scan: 0..%d", LED_COUNT - 1);
  for (int i = 0; i < LED_COUNT; i++) {
    clear();
    set(i, rgb(0, 255, 120));
    show();
    LOG_V("LED", "logical=%d -> physical=%u", i, (unsigned)physicalOf(i));
    delay(110);
  }
  clear();
  show();
}

// ============================================================
//  Display
// ============================================================

void Display::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  _ok = _oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR, true, false);
  if (!_ok) {
    LOG_W("OLED", "addr 0x%02X not found, trying 0x3D", OLED_ADDR);
    _ok = _oled.begin(SSD1306_SWITCHCAPVCC, 0x3D, true, false);
  }
  if (!_ok) {
    LOG_E("OLED", "SSD1306 not found! check SDA=%d SCL=%d", PIN_OLED_SDA, PIN_OLED_SCL);
  } else {
    LOG_I("OLED", "SSD1306 OK");
  }
  _oled.clearDisplay();
  _oled.display();
  _oled.setTextColor(SSD1306_WHITE);
  _oled.setTextWrap(false);
}

void Display::clear() { _oled.clearDisplay(); }

void Display::flush() {
  uint32_t now = millis();
  if (now - _lastFlush >= OLED_MIN_MS) {
    _lastFlush = now;
    _oled.display();
  }
}

void Display::flushNow() {
  _lastFlush = millis();
  _oled.display();
}

int Display::textWidth(const char* s, uint8_t size) {
  return (int)strlen(s) * 6 * size;
}

void Display::text(int x, int y, const char* s, uint8_t size) {
  _oled.setTextSize(size);
  _oled.setCursor(x, y);
  _oled.print(s);
}

void Display::textCentered(int y, const char* s, uint8_t size) {
  textCenteredIn(0, OLED_WIDTH - 1, y, s, size);
}

void Display::textCenteredIn(int x0, int x1, int y, const char* s, uint8_t size) {
  int w = textWidth(s, size);
  int x = x0 + ((x1 - x0 + 1) - w) / 2;
  if (x < x0) x = x0;
  text(x, y, s, size);
}

// ============================================================
//  Output: бузер через LEDC + вибромотор
// ============================================================

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define BUZZ_ATTACH()  ledcAttach(PIN_BUZZER, 2000, 8)
  #define BUZZ_TONE(f)   ledcWriteTone(PIN_BUZZER, (f))
  #define BUZZ_DUTY(d)   ledcWrite(PIN_BUZZER, (d))
#else
  #define BUZZER_CH 0
  #define BUZZ_ATTACH()  ledcSetup(BUZZER_CH, 2000, 8), ledcAttachPin(PIN_BUZZER, BUZZER_CH)
  #define BUZZ_TONE(f)   ledcWriteTone(BUZZER_CH, (f))
  #define BUZZ_DUTY(d)   ledcWrite(BUZZER_CH, (d))
#endif

// ---- банк звуковых эффектов ----
static const Note SFX_SELECT[]   = { {1047, 40}, {1568, 60} };
static const Note SFX_BACK[]     = { {1047, 40}, {659, 70} };
static const Note SFX_ERROR[]    = { {220, 90}, {0, 40}, {196, 140} };
static const Note SFX_EXPLODE[]  = { {180, 60}, {130, 70}, {95, 110} };
static const Note SFX_PICKUP[]   = { {1319, 40}, {1760, 50} };
static const Note SFX_LEVELUP[]  = { {784, 60}, {988, 60}, {1319, 60}, {1760, 110} };
static const Note SFX_WIN[]      = { {784, 80}, {988, 80}, {1319, 80}, {1568, 220} };
static const Note SFX_LOSE[]     = { {392, 110}, {330, 110}, {262, 220} };
static const Note SFX_FANFARE[]  = { {523, 70}, {659, 70}, {784, 70}, {1047, 90},
                                     {784, 60}, {1047, 260} };
static const Note SFX_GAMEOVER[] = { {440, 140}, {392, 140}, {349, 140}, {262, 380} };
static const Note SFX_COUNT[]    = { {880, 90}, {0, 210}, {880, 90}, {0, 210},
                                     {880, 90}, {0, 210}, {1760, 300} };

void Output::begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  BUZZ_ATTACH();
  BUZZ_DUTY(0);
  pinMode(PIN_VIBRO, OUTPUT);
  digitalWrite(PIN_VIBRO, LOW);
  LOG_I("OUT", "buzzer pin=%d, vibro pin=%d", PIN_BUZZER, PIN_VIBRO);
}

void Output::writeTone(uint16_t freq) {
#if SOUND_ENABLED
  if (freq == 0) { BUZZ_DUTY(0); return; }
  BUZZ_TONE(freq);
  BUZZ_DUTY(BUZZER_DUTY);
#else
  (void)freq;
#endif
}

void Output::silence() {
  _melody = nullptr;
  _sliding = false;
  _toneEnd = 0;
  BUZZ_DUTY(0);
}

void Output::update() {
  uint32_t now = millis();

  if (_sliding) {
    uint32_t el = now - _slideStart;
    if (el >= _slideDur) {
      _sliding = false;
      BUZZ_DUTY(0);
    } else {
      uint16_t f = (uint16_t)((int32_t)_slideFrom +
          ((int32_t)_slideTo - (int32_t)_slideFrom) * (int32_t)el / (int32_t)_slideDur);
      writeTone(f);
    }
  } else if (_melody) {
    if (now >= _toneEnd) {
      if (_melIdx >= _melCount) {
        _melody = nullptr;
        BUZZ_DUTY(0);
      } else {
        const Note& n = _melody[_melIdx++];
        writeTone(n.freq);
        _toneEnd = now + n.durMs;
      }
    }
  } else if (_toneEnd && now >= _toneEnd) {
    BUZZ_DUTY(0);
    _toneEnd = 0;
  }

  // Вибромотор: цепочка импульсов, полностью неблокирующая.
  if (_vibroLeft || _vibroState) {
    if (now >= _vibroNext) {
      if (_vibroState) {
        digitalWrite(PIN_VIBRO, LOW);
        _vibroState = false;
        if (_vibroLeft) _vibroNext = now + _vibroOff;
      } else if (_vibroLeft) {
        _vibroLeft--;
#if VIBRO_ENABLED
        digitalWrite(PIN_VIBRO, HIGH);
#endif
        _vibroState = true;
        _vibroNext = now + _vibroOn;
      }
    }
  }
}

void Output::tone(uint16_t freq, uint16_t durMs) {
  _melody = nullptr;
  _sliding = false;
  writeTone(freq);
  _toneEnd = millis() + durMs;
}

void Output::slide(uint16_t from, uint16_t to, uint16_t durMs) {
  _melody = nullptr;
  _toneEnd = 0;
  _sliding = true;
  _slideStart = millis();
  _slideDur = durMs ? durMs : 1;
  _slideFrom = from;
  _slideTo = to;
}

void Output::melody(const Note* notes, uint8_t count) {
  _sliding = false;
  _melody = notes;
  _melCount = count;
  _melIdx = 0;
  _toneEnd = 0;
}

void Output::buzz(uint16_t durMs) { buzzPattern(1, durMs, 0); }

void Output::buzzPattern(uint8_t pulses, uint16_t onMs, uint16_t offMs) {
  _vibroLeft = pulses;
  _vibroOn = onMs;
  _vibroOff = offMs;
  _vibroState = false;
  _vibroNext = millis();
}

void Output::sfx(Sfx id) {
  switch (id) {
    case Sfx::Click:        tone(988, 25); break;
    case Sfx::Beep:         tone(1319, 55); break;
    case Sfx::Select:       melody(SFX_SELECT, 2); break;
    case Sfx::Back:         melody(SFX_BACK, 2); break;
    case Sfx::Error:        melody(SFX_ERROR, 3); buzz(180); break;
    case Sfx::Shoot:        slide(1900, 900, 70); break;
    case Sfx::ShootCharged: slide(2400, 700, 150); buzz(40); break;
    case Sfx::Hit:          tone(330, 90); buzz(150); break;
    case Sfx::Explode:      melody(SFX_EXPLODE, 3); buzzPattern(2, 70, 50); break;
    case Sfx::Bounce:       tone(1568, 35); break;
    case Sfx::Twang:        slide(300, 1400, 110); buzz(35); break;
    case Sfx::Tick:         tone(2200, 14); break;
    case Sfx::Pickup:       melody(SFX_PICKUP, 2); break;
    case Sfx::LevelUp:      melody(SFX_LEVELUP, 4); buzz(90); break;
    case Sfx::Win:          melody(SFX_WIN, 4); buzzPattern(2, 110, 80); break;
    case Sfx::Lose:         melody(SFX_LOSE, 3); buzz(220); break;
    case Sfx::Fanfare:      melody(SFX_FANFARE, 6); buzzPattern(2, 90, 90); break;
    case Sfx::GameOver:     melody(SFX_GAMEOVER, 4); buzzPattern(3, 90, 70); break;
    case Sfx::Countdown:    melody(SFX_COUNT, 7); break;
  }
}

// ============================================================
//  Button
// ============================================================

void Button::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT_PULLUP);
}

void Button::update() {
  bool nowRaw = digitalRead(_pin) == LOW;   // активный низкий уровень
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
    _nextRepeat = 0;
  }
  if (_released) _lastHeld = now - _pressStart;

  _raw = stable;

  if (_raw && !_longFired && (now - _pressStart) >= BTN_LONG_MS) {
    _longFired = true;
    _longFlag = true;
  }
}

uint32_t Button::heldMs() const {
  return _raw ? (millis() - _pressStart) : 0;
}

bool Button::longPress() {
  bool r = _longFlag;
  _longFlag = false;
  return r;
}

bool Button::repeat(uint16_t firstMs, uint16_t rateMs) {
  if (_pressed) { _nextRepeat = millis() + firstMs; return true; }
  if (_raw && _nextRepeat && millis() >= _nextRepeat) {
    _nextRepeat = millis() + rateMs;
    return true;
  }
  return false;
}

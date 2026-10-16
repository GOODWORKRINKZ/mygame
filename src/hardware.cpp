#include "hardware.h"
#include "font_ru.h"
#include <math.h>
#include "driver/gpio.h"

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

  // 2D-геометрия: каждый физический светодиод должен стоять в сетке ровно
  // один раз, пути не должны ссылаться за пределы ленты.
  {
    uint8_t seen[LED_PHYSICAL_COUNT] = {0};
    for (int r = 0; r < LED_GRID_ROWS; r++)
      for (int c = 0; c < LED_GRID_COLS; c++) {
        uint8_t p = LED_GRID[r][c];
        if (p == LED_NONE) continue;
        if (p >= LED_PHYSICAL_COUNT) { LOG_E("LED", "grid[%d][%d]=%u вне ленты", r, c, p); continue; }
        if (seen[p]++) LOG_W("LED", "grid: физический %u встречается дважды", p);
      }
    for (int p = 0; p < LED_PHYSICAL_COUNT; p++)
      if (!seen[p]) LOG_W("LED", "grid: физический %d не размещён в сетке", p);
    const Path* paths[] = { &ARM_P1, &ARM_P2, &RING, &CORE };
    const char* names[] = { "ARM_P1", "ARM_P2", "RING", "CORE" };
    for (int k = 0; k < 4; k++)
      for (int i = 0; i < paths[k]->len; i++)
        if (paths[k]->idx[i] >= LED_PHYSICAL_COUNT)
          LOG_E("LED", "%s[%d]=%u вне ленты", names[k], i, paths[k]->idx[i]);
  }

  _strip.begin();
  _strip.setBrightness(LED_BRIGHTNESS);
  _strip.clear();
  _strip.show();
  resetWall();
  clear();
  LOG_I("LED", "strip ready: pin=%d physical=%d logical=%d walls=%d gamma=%d",
        PIN_LED, LED_PHYSICAL_COUNT, LED_COUNT, walls, (int)LED_GAMMA);
  LOG_I("LED", "geometry: grid %dx%d, arms %d+%d, ring %d, core %d",
        LED_GRID_COLS, LED_GRID_ROWS, ARM_P1.len, ARM_P2.len, RING.len, CORE.len);
}

void LedStrip::resetWall() { setWall(rgb(WALL_R, WALL_G, WALL_B)); }

void LedStrip::setWall(Color c) {
  _wall = c;
  paintWalls();
}

void LedStrip::setFullCanvas(bool on) {
  if (_full == on) return;
  _full = on;
  clear();
}

void LedStrip::paintWalls() {
  if (_full) return;
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++)
    if (_phys2log[p] == NO_LOGICAL) _buf[p] = _wall;
}

uint8_t LedStrip::physicalOf(int logical) const {
  return (logical >= 0 && logical < LED_COUNT) ? LED_LOGICAL_MAP[logical] : 0;
}

bool LedStrip::isWall(int physical) const {
  return (physical < 0 || physical >= LED_PHYSICAL_COUNT)
         || _phys2log[physical] == NO_LOGICAL;
}

// ---- кадр целиком ----

void LedStrip::clear() {
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) _buf[p] = 0;
  paintWalls();
}

void LedStrip::fade(uint8_t keep) {
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) _buf[p] = colScale(_buf[p], keep);
  paintWalls();
}

void LedStrip::fillAll(Color c) {
  if (_full) { for (int p = 0; p < LED_PHYSICAL_COUNT; p++) _buf[p] = c; }
  else       fill(0, LED_COUNT - 1, c);
}

void LedStrip::addAll(Color c) {
  if (_full) { for (int p = 0; p < LED_PHYSICAL_COUNT; p++) addP(p, c); }
  else       for (int i = 0; i < LED_COUNT; i++) add(i, c);
}

// ---- физические ----

void LedStrip::setP(int p, Color c) {
  if (p < 0 || p >= LED_PHYSICAL_COUNT) return;
  _buf[p] = c;
}

void LedStrip::addP(int p, Color c) {
  if (p < 0 || p >= LED_PHYSICAL_COUNT) return;
  uint16_t r = colR(_buf[p]) + colR(c);
  uint16_t g = colG(_buf[p]) + colG(c);
  uint16_t b = colB(_buf[p]) + colB(c);
  _buf[p] = rgb(r > 255 ? 255 : (uint8_t)r,
                g > 255 ? 255 : (uint8_t)g,
                b > 255 ? 255 : (uint8_t)b);
}

Color LedStrip::getP(int p) const {
  return (p < 0 || p >= LED_PHYSICAL_COUNT) ? 0 : _buf[p];
}

// ---- логическая линия ----

void LedStrip::set(int l, Color c) {
  if (l < 0 || l >= LED_COUNT) return;
  _buf[LED_LOGICAL_MAP[l]] = c;
}

void LedStrip::add(int l, Color c) {
  if (l < 0 || l >= LED_COUNT) return;
  addP(LED_LOGICAL_MAP[l], c);
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
  for (int i = from; i <= to; i++) _buf[LED_LOGICAL_MAP[i]] = c;
}

Color LedStrip::get(int l) const {
  return (l < 0 || l >= LED_COUNT) ? 0 : _buf[LED_LOGICAL_MAP[l]];
}

// ---- пути и маршруты ----

void LedStrip::addAA(const Path& p, float pos, Color c) {
  pos = p.wrap(pos);
  int i0 = (int)floorf(pos);
  float f = pos - (float)i0;
  uint8_t w1 = (uint8_t)(f * 255.0f);
  addP(p.phys(i0),     colScale(c, (uint8_t)(255 - w1)));
  addP(p.phys(i0 + 1), colScale(c, w1));
}

void LedStrip::fill(const Path& p, Color c) {
  for (int i = 0; i < p.len; i++) setP(p.idx[i], c);
}

void LedStrip::fade(const Path& p, uint8_t keep) {
  for (int i = 0; i < p.len; i++) {
    int ph = p.idx[i];
    if (ph >= 0 && ph < LED_PHYSICAL_COUNT) _buf[ph] = colScale(_buf[ph], keep);
  }
}

void LedStrip::addAA(const Route& r, float pos, Color c) {
  int i0 = (int)floorf(pos);
  float f = pos - (float)i0;
  uint8_t w1 = (uint8_t)(f * 255.0f);
  addP(r.phys(i0),     colScale(c, (uint8_t)(255 - w1)));
  addP(r.phys(i0 + 1), colScale(c, w1));
}

void LedStrip::fill(const Route& r, Color c) {
  int L = r.length();
  for (int i = 0; i < L; i++) setP(r.phys(i), c);
}

// ---- сетка ----

void LedStrip::addAAXY(float x, float y, Color c) {
  int cx = (int)floorf(x), cy = (int)floorf(y);
  float fx = x - (float)cx, fy = y - (float)cy;
  uint8_t wx = (uint8_t)(fx * 255.0f), wy = (uint8_t)(fy * 255.0f);
  addXY(cx,     cy,     colScale(colScale(c, (uint8_t)(255 - wx)), (uint8_t)(255 - wy)));
  addXY(cx + 1, cy,     colScale(colScale(c, wx),                 (uint8_t)(255 - wy)));
  addXY(cx,     cy + 1, colScale(colScale(c, (uint8_t)(255 - wx)), wy));
  addXY(cx + 1, cy + 1, colScale(colScale(c, wx),                 wy));
}

void LedStrip::fillRect(int col, int row, int w, int h, Color c) {
  for (int r = row; r < row + h; r++)
    for (int k = col; k < col + w; k++) setXY(k, r, c);
}

// ---- вывод ----

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
  for (int p = 0; p < LED_PHYSICAL_COUNT; p++) rawPixel(p, _buf[p]);
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
  _addr = OLED_ADDR;
  _ok = _oled.begin(SSD1306_SWITCHCAPVCC, _addr, true, false);
  if (!_ok) {
    LOG_W("OLED", "addr 0x%02X not found, trying 0x3D", OLED_ADDR);
    _addr = 0x3D;
    _ok = _oled.begin(SSD1306_SWITCHCAPVCC, _addr, true, false);
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
  _oled.setFont(&FontRu5x7);
}

// Строки в коде — UTF-8. Шрифт FontRu5x7 индексируется однобайтовыми кодами:
// ASCII как есть, а кириллица А..Я (U+0410..U+042F) — кодами 0x80..0x9F
// (см. include/font_ru.h). Строчные буквы и "Ё" не поддерживаются — весь
// интерфейс на экране пишется ЗАГЛАВНЫМИ.
static int utf8ToFontCodes(const char* s, uint8_t* out, int cap) {
  int n = 0;
  while (*s && n < cap - 1) {
    uint8_t b0 = (uint8_t)*s;
    uint32_t cp;
    if (b0 < 0x80) {
      cp = b0;
      s += 1;
    } else if ((b0 & 0xE0) == 0xC0 && s[1]) {
      cp = ((uint32_t)(b0 & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
      s += 2;
    } else {
      cp = '?';
      s += 1;
    }
    if (cp >= 0x0410 && cp <= 0x042F) out[n++] = (uint8_t)(0x80 + (cp - 0x0410));
    else if (cp < 0x80)               out[n++] = (uint8_t)cp;
    else                               out[n++] = '?';
  }
  out[n] = 0;
  return n;
}

void Display::clear() { _oled.clearDisplay(); }

// Кадр не отправляется целиком: display() у Adafruit держит шину ~25 мс,
// и всё это время loop() стоит — кнопки не опрашиваются, короткий тап
// теряется. Вместо этого снимаем копию кадра и отдаём её pump()'у по
// 127 байт за вызов (одна I2C-транзакция, ~3 мс).
void Display::flush() {
  if (!_ok) return;
  uint32_t now = millis();
  if (now - _lastFlush < OLED_MIN_MS) return;
  if (_txLeft) return;          // предыдущий кадр ещё уходит — не рвать его, доотправим
  _lastFlush = now;
  memcpy(_tx, _oled.getBuffer(), FRAME_BYTES);
  _txPos = 0;
  _txLeft = FRAME_BYTES;
  _txHeaderSent = false;
}

void Display::pump() {
  if (!_ok || _txLeft == 0) return;

  if (!_txHeaderSent) {
    // Adafruit после каждой своей транзакции возвращает шину на 100 кГц
    // (TRANSACTION_END -> clkAfter). Нам нужны 400 кГц, иначе кадр идёт
    // ~100 мс и не успевает дойти до нижних страниц между flush().
    Wire.setClock(400000);
    // Адресное окно на весь экран: страницы 0..7, столбцы 0..127.
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)0x00);                 // далее команды
    Wire.write((uint8_t)SSD1306_PAGEADDR);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0xFF);
    Wire.write((uint8_t)SSD1306_COLUMNADDR);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)(OLED_WIDTH - 1));
    Wire.endTransmission();
    _txHeaderSent = true;
    return;
  }

  uint16_t n = _txLeft < CHUNK ? _txLeft : CHUNK;
  Wire.beginTransmission(_addr);
  Wire.write((uint8_t)0x40);                   // далее данные
  Wire.write(_tx + _txPos, n);
  Wire.endTransmission();
  _txPos  += n;
  _txLeft -= n;
}

void Display::flushNow() {
  if (!_ok) return;
  _txLeft = 0;                                 // недосланный кадр больше не нужен
  _lastFlush = millis();
  _oled.display();
}

int Display::textWidth(const char* s, uint8_t size) {
  uint8_t buf[96];
  int n = utf8ToFontCodes(s, buf, sizeof buf);
  return n * 6 * size;
}

void Display::text(int x, int y, const char* s, uint8_t size) {
  uint8_t buf[96];
  int n = utf8ToFontCodes(s, buf, sizeof buf);
  _oled.setTextSize(size);
  _oled.setCursor(x, y);
  for (int i = 0; i < n; i++) _oled.write(buf[i]);
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

void Output::writeTone(uint16_t freq, uint8_t duty) {
#if SOUND_ENABLED
  if (freq == 0) { BUZZ_DUTY(0); return; }
  BUZZ_TONE(freq);
  BUZZ_DUTY(duty);
#else
  (void)freq; (void)duty;
#endif
}

void Output::silence() {
  _melody = nullptr;
  _sliding = false;
  _toneEnd = 0;
  _musicNote = -1;                  // музыка перезвучит свою ноту на следующем шаге
  BUZZ_DUTY(0);
}

// ---- музыка ----

void Output::music(const MusicTrack* track, bool loop) {
#if !MUSIC_ENABLED
  track = nullptr;
#endif
  if (track && (!track->notes || track->count == 0)) track = nullptr;
  _track = track;
  _musicLoop = loop;
  _musicPos = 0;
  _musicTempo = 1.0f;
  _musicLast = millis();
  _musicNote = -1;
  _musicGap = false;
  if (!track && !effectActive()) BUZZ_DUTY(0);
}

void Output::musicTempo(float mul) {
  if (mul < 0.25f) mul = 0.25f;
  if (mul > 4.0f)  mul = 4.0f;
  _musicTempo = mul;
}

// Позиция в теме идёт всегда, звук — только когда бузер свободен от эффектов.
void Output::musicUpdate(uint32_t now) {
  if (!_track) return;
  uint32_t dt = now - _musicLast;
  _musicLast = now;
  _musicPos += (float)dt * _musicTempo;

  // длина темы и нота под текущей позицией
  float total = 0;
  for (uint8_t i = 0; i < _track->count; i++) total += _track->notes[i].durMs;
  if (total <= 0) return;
  if (_musicPos >= total) {
    if (!_musicLoop) {                       // мотив отыгран — замолкаем
      _track = nullptr;
      _musicNote = -1;
      if (!effectActive()) BUZZ_DUTY(0);
      return;
    }
    while (_musicPos >= total) _musicPos -= total;
  }

  float acc = 0;
  int idx = 0;
  for (uint8_t i = 0; i < _track->count; i++) {
    if (_musicPos < acc + _track->notes[i].durMs) { idx = i; break; }
    acc += _track->notes[i].durMs;
  }
  const Note& n = _track->notes[idx];
  // короткая пауза в конце ноты — иначе две одинаковые сливаются в одну
  float gap = n.durMs > 4 * MUSIC_GAP_MS ? (float)MUSIC_GAP_MS : n.durMs / 4.0f;
  bool inGap = (_musicPos - acc) >= (n.durMs - gap);

  if (effectActive()) { _musicNote = -1; return; }   // бузер занят эффектом

  if (idx != _musicNote || inGap != _musicGap) {
    _musicNote = idx;
    _musicGap = inGap;
    if (inGap || n.freq == 0) BUZZ_DUTY(0);
    else                      writeTone(n.freq, MUSIC_DUTY);
  }
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

  musicUpdate(now);

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
  // ---- вибрация: общая для всех банков ----
  switch (id) {
    case Sfx::Error:        buzz(180); break;
    case Sfx::ShootCharged: buzz(40); break;
    case Sfx::Hit:          buzz(150); break;
    case Sfx::Explode:      buzzPattern(2, 70, 50); break;
    case Sfx::Twang:        buzz(35); break;
    case Sfx::LevelUp:      buzz(90); break;
    case Sfx::Win:          buzzPattern(2, 110, 80); break;
    case Sfx::Lose:         buzz(220); break;
    case Sfx::Fanfare:      buzzPattern(2, 90, 90); break;
    case Sfx::GameOver:     buzzPattern(3, 90, 70); break;
    default: break;
  }

  // ---- банк текущей игры: свой звук вместо стандартного ----
  if (_bank) {
    for (uint8_t i = 0; i < _bank->count; i++) {
      const SfxOverride& o = _bank->items[i];
      if (o.id != id) continue;
      if (o.notes)        melody(o.notes, o.count);
      else if (o.slideMs) slide(o.slideFrom, o.slideTo, o.slideMs);
      return;
    }
  }

  // ---- стандартный набор ----
  switch (id) {
    case Sfx::Click:        tone(988, 25); break;
    case Sfx::Beep:         tone(1319, 55); break;
    case Sfx::Select:       melody(SFX_SELECT, 2); break;
    case Sfx::Back:         melody(SFX_BACK, 2); break;
    case Sfx::Error:        melody(SFX_ERROR, 3); break;
    case Sfx::Shoot:        slide(1900, 900, 70); break;
    case Sfx::ShootCharged: slide(2400, 700, 150); break;
    case Sfx::Hit:          tone(330, 90); break;
    case Sfx::Explode:      melody(SFX_EXPLODE, 3); break;
    case Sfx::Bounce:       tone(1568, 35); break;
    case Sfx::Twang:        slide(300, 1400, 110); break;
    case Sfx::Tick:         tone(2200, 14); break;
    case Sfx::Pickup:       melody(SFX_PICKUP, 2); break;
    case Sfx::LevelUp:      melody(SFX_LEVELUP, 4); break;
    case Sfx::Win:          melody(SFX_WIN, 4); break;
    case Sfx::Lose:         melody(SFX_LOSE, 3); break;
    case Sfx::Fanfare:      melody(SFX_FANFARE, 6); break;
    case Sfx::GameOver:     melody(SFX_GAMEOVER, 4); break;
    case Sfx::Countdown:    melody(SFX_COUNT, 7); break;
  }
}

// ============================================================
//  Button
// ============================================================

// ---- очередь событий: пишет ISR, читает loop ----
struct BtnEvent {
  uint32_t ms;      // millis() в момент прерывания
  uint8_t  pin;
  uint8_t  low;     // 1 = нога на GND (нажата)
};
static const uint8_t EVQ = 64;                       // степень двойки
static volatile BtnEvent  s_q[EVQ];
static volatile uint8_t   s_head = 0, s_tail = 0;    // один писатель (ISR), один читатель
static volatile uint32_t  s_dropped = 0;

Button* Button::s_all[Button::MAX_BUTTONS] = { nullptr, nullptr, nullptr, nullptr };
uint8_t Button::s_count = 0;

uint32_t Button::droppedEvents() { return s_dropped; }

void IRAM_ATTR Button::isr(void* arg) {
  Button* b = (Button*)arg;
  uint8_t h = s_head;
  uint8_t n = (uint8_t)((h + 1) & (EVQ - 1));
  if (n == s_tail) { s_dropped++; return; }          // переполнение — событие пропадёт,
                                                     // settle() достроит фронт по ноге
  s_q[h].ms  = millis();
  s_q[h].pin = b->_pin;
  s_q[h].low = (gpio_get_level((gpio_num_t)b->_pin) == 0) ? 1 : 0;
  s_head = n;
}

void Button::begin(uint8_t pin) {
  _pin = pin;
  pinMode(_pin, INPUT_PULLUP);
  _lastDebounceRaw = (digitalRead(_pin) == LOW);
  _lastDebounceTime = millis();
  if (s_count < MAX_BUTTONS) s_all[s_count++] = this;
  else LOG_E("BTN", "too many buttons, pin %u polled without IRQ", _pin);
  attachInterruptArg(digitalPinToInterrupt(_pin), &Button::isr, this, CHANGE);
}

// Раздать все накопившиеся события их кнопкам, в хронологическом порядке.
void Button::drain() {
  while (s_tail != s_head) {
    uint8_t t = s_tail;
    uint32_t ms = s_q[t].ms;
    uint8_t pin = s_q[t].pin;
    bool low = s_q[t].low != 0;
    s_tail = (uint8_t)((t + 1) & (EVQ - 1));
    for (uint8_t i = 0; i < s_count; i++)
      if (s_all[i] && s_all[i]->_pin == pin) { s_all[i]->feed(low, ms); break; }
  }
}

// Фронт на ноге в момент ms (из прерывания или достроенный сверкой).
void Button::feed(bool low, uint32_t ms) {
  if (low == _lastDebounceRaw) return;             // уровень тот же — пульс был короче ISR
  if (_raw && !low) _glitches++;                   // контакт разомкнулся во время удержания
  _lastDebounceRaw = low;
  _lastDebounceTime = ms;
}

void Button::update() {
  drain();
  uint32_t now = millis();
  // Сверка с ногой: если событие потерялось, достраиваем фронт "сейчас".
  bool cur = digitalRead(_pin) == LOW;
  if (cur != _lastDebounceRaw) feed(cur, now);
  settle(now);
}

void Button::settle(uint32_t now) {
  bool nowRaw = _lastDebounceRaw;
  // Нажатие подтверждаем быстро. Отпускание — дольше, и тем дольше, чем
  // дольше держали: короткий тап при долбеже отпускается за BTN_RELEASE_MS,
  // а долгое удержание (заряд, блок, полоска в ЖАДНОСТИ) не рвётся от
  // случайного размыкания контакта короче BTN_RELEASE_HOLD_MS.
  uint32_t need = BTN_PRESS_MS;
  if (!nowRaw) {
    bool longHold = _raw && (now - _pressStart) >= BTN_HOLD_MS;
    need = longHold ? BTN_RELEASE_HOLD_MS : BTN_RELEASE_MS;
  }
  bool stable = (now - _lastDebounceTime >= need) ? nowRaw : _raw;

  _pressed  = stable && !_raw;
  _released = !stable && _raw;

  if (_pressed) {
    _pressStart = _lastDebounceTime;               // время самого фронта, не кадра
    _longFired = false;
    _longFlag = false;
    _stuckWarned = false;
    _nextRepeat = 0;
  }
  if (_released) _lastHeld = _lastDebounceTime - _pressStart;

  _raw = stable;

  if (_raw && !_longFired && (now - _pressStart) >= BTN_LONG_MS) {
    _longFired = true;
    _longFlag = true;
  }

  // Кнопка "нажата" неправдоподобно долго — почти наверняка залипла
  // физически. Пишем один раз за нажатие, чтобы было видно в логе.
  if (_raw && !_stuckWarned && (now - _pressStart) >= BTN_STUCK_WARN_MS) {
    _stuckWarned = true;
    LOG_W("BTN", "pin %u reads LOW for %lu ms — stuck button? (wiring NC/NO, cap, short)",
          _pin, (unsigned long)(now - _pressStart));
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

bool Button::repeat(bool pressedNow, uint16_t firstMs, uint16_t rateMs) {
  uint32_t now = millis();
  if (pressedNow) { _nextRepeat = now + firstMs; return true; }
  if (_raw && _nextRepeat && now >= _nextRepeat) {
    _nextRepeat = now + rateMs;
    return true;
  }
  return false;
}

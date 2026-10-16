#pragma once
#include <Arduino.h>
#include "led_map.h"

// ============================================================
//  ВИРТУАЛЬНЫЕ ДИСПЛЕИ ПОВЕРХ ФИЗИЧЕСКОЙ ЛЕНТЫ
// ============================================================
//
//  Игра не обязана думать о ленте как о прямой линии. Поверх
//  физических светодиодов можно описать сколько угодно "дисплеев":
//
//   * Path  — одномерный дисплей: упорядоченный список физических
//             светодиодов, по которому можно ехать дробной
//             координатой (как раньше по логической ленте). Путь
//             может быть замкнут (кольцо вокруг матрицы).
//   * Route — маршрут, склеенный из кусков путей: "рукав зелёного,
//             потом два круга по кольцу, потом рукав синего".
//             Снаружи — одна сплошная координата 0..length().
//   * Grid  — двумерная сетка LED_GRID из led_map.h: setXY/addXY
//             на LedStrip рисуют по (столбец, ряд).
//
//  Пути могут пересекаться: один и тот же физический светодиод
//  может лежать и на старой логической линии, и на кольце, и в
//  рукаве. Смешивание — аддитивное (LedStrip::add).
//
//  Сама геометрия (какие светодиоды где) описана в led_map.h,
//  здесь только механика.
// ============================================================

struct Path {
  const uint8_t* idx;   // физические номера по порядку
  uint8_t        len;
  bool           loop;  // замкнут ли путь (последний соседствует с первым)

  constexpr Path(const uint8_t* i, uint8_t n, bool l) : idx(i), len(n), loop(l) {}

  // Физический светодиод i-й клетки. На кольце индекс заворачивается,
  // на отрезке за краями возвращает -1.
  int phys(int i) const {
    if (len == 0) return -1;
    if (loop) {
      i %= (int)len;
      if (i < 0) i += len;
      return idx[i];
    }
    return (i < 0 || i >= (int)len) ? -1 : idx[i];
  }

  // Нормализовать дробную координату на кольце в [0, len).
  float wrap(float pos) const {
    if (!loop || len == 0) return pos;
    while (pos >= (float)len) pos -= (float)len;
    while (pos < 0.0f)        pos += (float)len;
    return pos;
  }

  // Расстояние "вперёд" по кольцу от a до b (0..len).
  float ahead(float a, float b) const { return wrap(b - a); }

  bool contains(int physical) const {
    for (uint8_t i = 0; i < len; i++) if (idx[i] == physical) return true;
    return false;
  }
};

template <size_t N>
constexpr Path makePath(const uint8_t (&arr)[N], bool loop = false) {
  return Path(arr, (uint8_t)N, loop);
}

// ---- Готовые пути текущей раскладки (см. led_map.h) ----
static const Path PATH_LINE   = Path(LED_LOGICAL_MAP, LED_COUNT, false);  // старая логическая лента
static const Path ARM_P1      = makePath(LED_ARM_P1);                     // от зелёной кнопки к матрице
static const Path ARM_P2      = makePath(LED_ARM_P2);                     // от синей кнопки к матрице
static const Path RING        = makePath(LED_RING, true);                 // периметр матрицы, по часовой
static const Path CORE        = makePath(LED_CORE, true);                 // центральные 2x2

// ============================================================
//  Route — маршрут из кусков путей
// ============================================================
//  Кусок: путь, стартовая клетка, сколько клеток пройти, направление.
//  На кольце count может быть больше len — это и есть "круги".
// ============================================================
struct RouteSeg {
  const Path* path;
  int16_t     start;
  int16_t     count;
  int8_t      dir;      // +1 или -1
};

struct Route {
  static const uint8_t MAX_SEGS = 4;
  RouteSeg segs[MAX_SEGS];
  uint8_t  n = 0;

  void clear() { n = 0; }
  void add(const Path& p, int start, int count, int dir = 1) {
    if (n >= MAX_SEGS || count <= 0) return;
    segs[n++] = { &p, (int16_t)start, (int16_t)count, (int8_t)(dir < 0 ? -1 : 1) };
  }

  int length() const {
    int L = 0;
    for (uint8_t i = 0; i < n; i++) L += segs[i].count;
    return L;
  }

  // Физический светодиод клетки cell (0..length()-1), иначе -1.
  int phys(int cell) const {
    if (cell < 0) return -1;
    for (uint8_t i = 0; i < n; i++) {
      const RouteSeg& s = segs[i];
      if (cell < s.count) return s.path->phys(s.start + s.dir * cell);
      cell -= s.count;
    }
    return -1;
  }

  // В каком куске находится клетка (для логики "мы уже на кольце?").
  int segOf(int cell) const {
    if (cell < 0) return -1;
    for (uint8_t i = 0; i < n; i++) {
      if (cell < segs[i].count) return i;
      cell -= segs[i].count;
    }
    return -1;
  }

  // С какой клетки маршрута начинается кусок i.
  int segStart(uint8_t i) const {
    int L = 0;
    for (uint8_t k = 0; k < i && k < n; k++) L += segs[k].count;
    return L;
  }
};

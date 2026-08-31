#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  СЛОЙ ЛОГИЧЕСКОЙ АДРЕСАЦИИ СВЕТОДИОДОВ (LUT)
// ============================================================
//  Физически лента может быть собрана из нескольких панелей,
//  часть из которых припаяна "задом наперёд". Чтобы в коде всегда
//  работать с логическим номером светодиода 0..31 (слева направо),
//  мы строим таблицу переадресации: логический индекс -> физический.

// Одна панель описывается сегментом.
struct LedSegment {
  uint8_t logicalStart;   // первый логический индекс сегмента
  uint8_t length;         // сколько светодиодов
  uint8_t physicalStart;  // первый физический индекс на ленте
  bool    reversed;       // физически панель идёт в обратную сторону
};

// ---- ВАША РАСКЛАДКА ПАНЕЛЕЙ (отредактируйте под реальную ленту) ----
// По умолчанию: одна цельная панель 32 светодиода, без реверса.
//
// Пример: 4 панели по 8 светодиодов, 2-я панель припаяна наоборот:
//   static const LedSegment PANELS[] = {
//     {  0, 8,  0, false },
//     {  8, 8,  8, true  },  // 2-я панель перевёрнута
//     { 16, 8, 16, false },
//     { 24, 8, 24, false },
//   };
static const LedSegment PANELS[] = {
  { 0, LED_COUNT, 0, false }
};
static const uint8_t PANEL_COUNT = sizeof(PANELS) / sizeof(PANELS[0]);

class LedMap {
public:
  static const uint8_t COUNT = LED_COUNT;
  uint8_t lut[COUNT];   // логический -> физический

  LedMap() { identity(); }

  void identity() {
    for (uint8_t i = 0; i < COUNT; i++) lut[i] = i;
  }

  // Строит LUT из массива сегментов.
  void build(const LedSegment* segs, uint8_t n) {
    identity();
    for (uint8_t s = 0; s < n; s++) {
      const LedSegment& g = segs[s];
      for (uint8_t i = 0; i < g.length; i++) {
        uint8_t logical = g.logicalStart + i;
        uint8_t phys = g.reversed
            ? g.physicalStart + (g.length - 1 - i)
            : g.physicalStart + i;
        if (logical < COUNT) lut[logical] = phys;
      }
    }
  }

  uint8_t operator[](uint8_t logical) const {
    return logical < COUNT ? lut[logical] : 0;
  }
};

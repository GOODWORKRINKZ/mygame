#pragma once
#include <Arduino.h>
#include "hardware.h"

// ============================================================
//  Интерфейс на OLED 128x64.
//
//  Основная раскладка для игр на двоих — экран поделён пополам:
//  слева ЗЕЛЁНЫЙ игрок, справа СИНИЙ.
//
//   0                63 64               127
//  +-------------------+------------------+  y=0
//  | TUG OF WAR            R2/3           |  шапка
//  +-------------------+------------------+  y=10
//  |      GREEN        |       BLUE       |  y=12  имя
//  |        2          |         1        |  y=21  крупно
//  |     COMBO x3      |     COMBO x1     |  y=39  подпись
//  |   [||||||    ]    |   [||        ]   |  y=48  полоска
//  +-------------------+------------------+  y=53
//  | pull the rope out of the middle      |  подвал
//  +--------------------------------------+  y=63
// ============================================================

struct Panel {
  const char* name = "";
  const char* big  = "";     // крупное значение (счёт, HP, время)
  const char* sub  = "";     // мелкая подпись
  int  bar    = -1;          // 0..100, -1 = полоску не рисовать
  int  dots   = -1;          // сколько кружков-побед показать (0..ROUNDS_TO_WIN)
  int  dotsMax = 0;
  bool active = false;       // подсветить панель (инверсия имени)
};

namespace Ui {

// --- примитивы ---
void header(Display& d, const char* left, const char* right);
void footer(Display& d, const char* text);
void divider(Display& d);
void bar(Display& d, int x, int y, int w, int h, int pct);
void dotsRow(Display& d, int cx, int y, int total, int filled);

// --- готовые экраны ---
// Игра на двоих: две колонки.
void split(Display& d, const char* title, const char* right,
           const Panel& left, const Panel& rightPanel, const char* foot);

// Игра на одного: один крупный блок по центру.
void solo(Display& d, const char* title, const char* right,
          const char* big, const char* line1, const char* line2, const char* foot);

// Большая надпись во весь экран (победа, GAME OVER, обратный отсчёт).
void banner(Display& d, const char* big, const char* sub, const char* foot);

// Список игр в меню.
void menuFrame(Display& d, const char* title, const char* const* items,
               const uint8_t* playerCounts, int count, int sel, const char* foot);

// Экран-заставка при включении, phase 0..2.
void bootFrame(Display& d, uint8_t phase, uint8_t progress);

}  // namespace Ui

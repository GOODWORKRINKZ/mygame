#pragma once

// ============================================================
//  LED ARCADE — вся настройка в одном файле
// ============================================================

// ---------------- Распиновка (ESP32-C3) ----------------
//  НЕ используйте GPIO2/8/9 (страппинг), GPIO18/19 (USB),
//  GPIO20/21 (UART0) — на большинстве плат они заняты.
#define PIN_LED        10   // DATA ленты WS2812 (через резистор 330-470 Ом)
#define PIN_OLED_SDA    4
#define PIN_OLED_SCL    5
#define PIN_BTN_P1      0   // зелёная кнопка (игрок 1, левый край ленты)
#define PIN_BTN_P2      1   // синяя кнопка  (игрок 2, правый край ленты)
#define PIN_BTN_MENU    3   // маленькая кнопка управления
#define PIN_BUZZER      7   // бузер (через транзистор)
#define PIN_VIBRO       6   // вибромоторчик (через транзистор)

// ---------------- Светодиодная лента ----------------
// Размеры ленты и карта "логический -> физический" живут в led_map.h:
// оттуда приходят LED_PHYSICAL_COUNT, LED_COUNT и цвет стен WALL_*.
#include "led_map.h"

#define LED_BRIGHTNESS  80        // 0..255 глобальная яркость
#define LED_GAMMA       1         // 1 = гамма-коррекция (плавные затухания)

// Средняя зона (нужна перетягиванию каната) — всегда по центру поля,
// какой бы длины оно ни получилось.
#define MIDDLE_LEN      16
#define MIDDLE_LO       ((LED_COUNT - MIDDLE_LEN) / 2)
#define MIDDLE_HI       (MIDDLE_LO + MIDDLE_LEN - 1)

// ---------------- Тайминги движка ----------------
#define FRAME_MS        16        // ~60 кадров/с на ленте
#define OLED_MIN_MS     60        // OLED перерисовываем не чаще (I2C медленный)

// ---------------- OLED 128x64 I2C ----------------
#define OLED_ADDR       0x3C      // чаще 0x3C, иногда 0x3D (код пробует оба)
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

// ---------------- Кнопки ----------------
#define BTN_DEBOUNCE_MS 8
#define BTN_LONG_MS     700

// ---------------- Звук и вибро ----------------
#define SOUND_ENABLED   1
#define VIBRO_ENABLED   1
#define BUZZER_DUTY     120       // скважность ШИМ 0..255 = "громкость"

// ============================================================
//  Параметры игр
// ============================================================
#define ROUNDS_TO_WIN      3      // побед в матче (для игр на двоих)

// --- TUG OF WAR (перетягивание) ---
#define TUG_STEP           0.42f  // сдвиг каната за одно нажатие, пикселей
#define TUG_COMBO_MS       280    // окно комбо: жмёшь чаще — тянешь сильнее
#define TUG_COMBO_MAX      6

// --- SHOOTER (стрелялка) ---
#define SHOOTER_HP         5
#define SHOT_SPEED         14.0f  // пикселей в секунду
#define SHOT_SPEED_CHARGED 22.0f
#define SHOT_COOLDOWN_MS   200
#define CHARGE_FULL_MS     650    // столько держать кнопку до заряженного выстрела
#define SHOT_MAX           8      // одновременных снарядов на поле

// --- PONG (шпингалет) ---
#define PONG_PADDLE_MAX    8.0f   // максимальный вылет шпингалета, пикселей
#define PONG_CHARGE_RATE   11.0f  // пикселей энергии в секунду при удержании
#define PONG_STRIKE_SPEED  70.0f  // скорость выброса шпингалета
#define PONG_RETURN_SPEED  26.0f  // скорость возврата
#define PONG_BALL_MIN      7.0f   // скорость мяча при слабом ударе
#define PONG_BALL_MAX      30.0f  // при полном заряде

// --- DUEL (дуэль на реакцию) ---
#define DUEL_WAIT_MIN      1200
#define DUEL_WAIT_MAX      4500

// --- SPRINT (гонка) ---
#define SPRINT_IMPULSE     3.2f   // прибавка скорости за нажатие, пикс/с
#define SPRINT_FRICTION    2.6f   // затухание скорости в секунду

// --- BOMB (горячая картошка) ---
#define BOMB_ZONE          6      // сколько пикселей у края — "твоя зона"
#define BOMB_SPEED_START   9.0f
#define BOMB_SPEED_MUL     1.09f  // ускорение после каждого отбива
#define BOMB_FUSE_MIN      7000
#define BOMB_FUSE_MAX      15000

// --- SNIPER (меткость, 1 игрок) ---
#define SNIPER_LIVES       3
#define SNIPER_ZONE_START  7
#define SNIPER_SPEED_START 15.0f

// --- SIMON (память, 1 игрок) ---
#define SIMON_MAX_LEN      40

// --- RUNNER (бегун, 1 игрок) ---
#define RUNNER_POS         4      // где стоит игрок
#define RUNNER_ACTION_MS   260    // длительность прыжка/подката
#define RUNNER_LIVES       3

// --- DEFENDER (оборона, 1 игрок) ---
#define DEFENDER_LIVES     3
#define DEFENDER_BASE      3      // база занимает пиксели [0..2]
#define DEFENDER_MAX_ENEMY 8
#define DEFENDER_NUKES     3

// ---------------- Диагностика ----------------
// 1 = при каждом включении гонять тесты ленты (медленно, ~6 с).
// В обычном режиме те же тесты доступны из меню: пункт "DIAGNOSTIC".
#define DIAG_ON_BOOT       0

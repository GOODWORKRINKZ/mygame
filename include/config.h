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
// Антидребезг несимметричный: замыкание на GND через подтяжку — сигнал
// однозначный, его подтверждаем быстро; отпускание дребезжит, его ждём
// чуть дольше. BTN_RELEASE_MS — это и минимальная пауза между двумя
// нажатиями: при быстром "долбеже" кнопка вверху бывает всего 15–25 мс,
// поэтому больше ~8 мс ставить нельзя — нажатия начнут склеиваться.
// Контакт, который после 8 мс покоя снова дёргается, уже не дребезг.
#define BTN_PRESS_MS     2        // сколько держать LOW, чтобы засчитать нажатие
#define BTN_RELEASE_MS   8        // сколько держать HIGH, чтобы засчитать отпускание
// Долгое удержание — другая история: палец "плывёт" по порогу микрика,
// и контакт может на десятки миллисекунд размыкаться (ЖАДНОСТЬ, блок в
// БОЮ, заряд в СТРЕЛКЕ). Если кнопка держится дольше BTN_HOLD_MS, для
// отпускания требуем BTN_RELEASE_HOLD_MS тишины. На долбёж это не влияет:
// там нажатия короткие.
#define BTN_HOLD_MS          250
#define BTN_RELEASE_HOLD_MS  60
// Если удержание всё равно "само отпускается", почти наверняка это не
// контакт, а земля: ток ленты (до ~2 А) через общий провод GND
// приподнимает "ноль" у кнопки, и нога читает HIGH, пока кнопка замкнута.
// Лечится проводкой (GND кнопок — отдельным проводом прямо к GND платы),
// а не этим числом; см. README, раздел "Кнопки".
#define BTN_LONG_MS      700
// Если кнопка читается нажатой дольше этого — скорее всего, залипла
// физически (контакт, конденсатор, замыкание). Пишем предупреждение в лог.
#define BTN_STUCK_WARN_MS 8000

// ---------------- Звук и вибро ----------------
#define SOUND_ENABLED   1
#define VIBRO_ENABLED   1
#define BUZZER_DUTY     120       // скважность ШИМ 0..255 = "громкость" эффектов
// Фоновая музыка: у каждой игры своя тема (Game::music), играет тише
// эффектов и уступает им бузер. 0 — выключить музыку целиком.
#define MUSIC_ENABLED   1
#define MUSIC_DUTY      40        // "громкость" музыки (меньше BUZZER_DUTY)
#define MUSIC_GAP_MS    25        // пауза между нотами, чтобы одинаковые не сливались

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

// --- ORBIT (орбита, 2 игрока: рукав -> кольцо матрицы -> рукав) ---
#define ORBIT_ZONE          3      // последние пиксели рукава у кнопки — "твоя зона"
#define ORBIT_SPEED_START   10.0f  // клеток в секунду
#define ORBIT_SPEED_MUL     1.07f  // ускорение после каждого отбива
#define ORBIT_LAPS_MAX      2      // максимум кругов по кольцу за один пролёт
#define ORBIT_STUN_MS       450    // штраф за удар вхолостую

// --- CATCH (ЛОВИ, 1 игрок, матрица 4x4) ---
#define CATCH_LIVES         3
#define CATCH_SPEED_START   2.2f   // рядов в секунду
#define CATCH_SPEED_UP      0.07f  // прибавка за пойманный блок
#define CATCH_SPEED_MAX     6.5f

// --- COLORS (ЦВЕТА, 1 игрок, матрица 4x4, три в ряд) ---
#define COLORS_SPEED_START  0.8f   // рядов в секунду
#define COLORS_SPEED_UP     0.03f  // прибавка за очко
#define COLORS_SPEED_MAX    2.6f
#define COLORS_CLEAR_MS     380    // сколько мигают исчезающие линии

// --- PONG 2D (понг на матрице, 2 игрока) ---
// Мяч ходит шагами по клеткам: на 4x4 плавное движение не читается.
#define PONG2D_STEP_START   420    // мс на одну клетку при подаче
#define PONG2D_STEP_UP      0.92f  // ускорение тика за каждый отбив
#define PONG2D_STEP_MIN     150

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

// --- TRAFFIC (СТОП-СИГНАЛ, 2 игрока) ---
#define TRAFFIC_WAIT_MIN     900     // пауза перед сигналом, мс
#define TRAFFIC_WAIT_MAX     3400
#define TRAFFIC_WINDOW_MS    700     // сколько горит сигнал
#define TRAFFIC_RED_PERCENT  45      // доля "красных" сигналов (жать нельзя)

// --- FIGHT (БОЙ, 2 игрока) ---
#define FIGHT_HP             4
#define FIGHT_STAMINA        3.0f    // полный запас стамины
#define FIGHT_GUARD_DRAIN    1.5f    // расход стамины в секунду при блоке
#define FIGHT_REGEN          1.0f    // восстановление в секунду
#define FIGHT_BLOCK_COST     0.9f    // стамина за отражённый удар
#define FIGHT_ATTACK_COST    0.6f    // стамина за удар
#define FIGHT_ATTACK_MS      420     // время полёта удара до соперника
#define FIGHT_COOLDOWN_MS    240
#define FIGHT_STUN_MS        700     // ступор после парирования / пробитого блока
#define FIGHT_GUARD_MS       130     // держишь дольше — это блок, короче — удар

// --- GREED (ЖАДНОСТЬ, 2 игрока) ---
#define GREED_MAX_LEN        13      // максимум пикселей в своей половине
#define GREED_RATE           5.0f    // пикселей в секунду при удержании
#define GREED_MINE_MIN       4       // ближе этого мина не ставится
#define GREED_ROUND_MS       14000   // общий лимит времени на раунд

// --- CHASE (ДОГОНЯЛКИ, 2 игрока, кольцо матрицы из 12 клеток) ---
#define CHASE_IMPULSE        2.6f    // прибавка скорости за нажатие, клеток/с
#define CHASE_FRICTION       2.5f
#define CHASE_HUNTER_BONUS   1.16f   // насколько охотник резвее беглеца
#define CHASE_TIME_MS        14000   // столько длится погоня
#define CHASE_CATCH_DIST     0.8f    // на таком расстоянии считается "поймал"

// --- HUNT (КЛАД, 2 игрока) ---
#define HUNT_SPEED_START     12.0f   // скорость курсора, пикс/с
#define HUNT_SPEED_UP        1.045f  // ускорение после каждого выстрела
#define HUNT_MAX_MARKS       16      // сколько отметок помним

// --- TOWER (БАШНЯ, 1 игрок) ---
#define TOWER_WIDTH_START    5
#define TOWER_SPEED_START    9.0f
#define TOWER_SPEED_UP       1.06f

// --- SNAKE (ЗМЕЙКА, 1 игрок, кольцо) ---
#define SNAKE_STEP_START     240     // мс на один шаг
#define SNAKE_STEP_MIN       95
#define SNAKE_STEP_UP        0.965f  // ускорение после каждой еды
#define SNAKE_START_LEN      3
#define SNAKE_MAX_LEN        24

// --- RHYTHM (РИТМ, 1 игрок) ---
#define RHYTHM_HIT_POS       3       // где стоит "наковальня"
#define RHYTHM_BEAT_START    620     // мс между долями
#define RHYTHM_BEAT_MIN      280
#define RHYTHM_BEAT_UP       0.94f   // ускорение темпа каждые 8 нот
#define RHYTHM_LIVES         3
#define RHYTHM_MAX_NOTES     8

// --- SYNC (СИНХРОН, кооператив) ---
#define SYNC_TOL_START       260     // допуск по времени, мс
#define SYNC_TOL_MIN         45
#define SYNC_TOL_UP          0.88f   // сужение допуска за уровень
#define SYNC_LIVES           3
#define SYNC_TRAVEL_MS       1500    // сколько летят импульсы до центра

// --- SIEGE (ОСАДА, кооператив) ---
#define SIEGE_LIVES          5
#define SIEGE_MAX_ENEMY      10
#define SIEGE_MAX_BULLETS    8
#define SIEGE_KILLS_PER_WAVE 10
#define SIEGE_COOLDOWN_MS    220
#define SIEGE_RING_SPEED_MUL 1.4f    // на кольце враг идёт быстрее, чем по рукаву

// ---------------- Диагностика ----------------
// 1 = при каждом включении гонять тесты ленты (медленно, ~6 с).
// В обычном режиме те же тесты доступны из меню: пункт "DIAGNOSTIC".
#define DIAG_ON_BOOT       0

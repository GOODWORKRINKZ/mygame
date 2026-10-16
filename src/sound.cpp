#include "sound.h"
#include "config.h"

// ---- ноты, Гц ----
#define N_C3  131
#define N_D3  147
#define N_Eb3 156
#define N_E3  165
#define N_F3  175
#define N_G3  196
#define N_A3  220
#define N_Bb3 233
#define N_B3  247
#define N_C4  262
#define N_Cs4 277
#define N_D4  294
#define N_Eb4 311
#define N_E4  330
#define N_F4  349
#define N_Fs4 370
#define N_G4  392
#define N_A4  440
#define N_Bb4 466
#define N_B4  494
#define N_C5  523
#define N_Cs5 554
#define N_D5  587
#define N_Eb5 622
#define N_E5  659
#define N_F5  698
#define N_G5  784
#define N_A5  880
#define N_B5  988
#define N_C6  1047
#define REST  0

// ---- длительности, мс (темп 1.0) ----
#define S   75     // шестнадцатая
#define E   150    // восьмая
#define Q   300    // четверть
#define DQ  450    // четверть с точкой
#define H   600    // половинная

// ------------------------------------------------------------
//  МЕНЮ — неторопливая, "витрина"
static const Note T_MENU[] = {
  {N_C5, Q}, {N_E5, Q}, {N_G5, Q}, {N_E5, Q}, {N_A4, Q}, {N_C5, Q}, {N_E5, H}, {REST, Q},
  {N_D5, Q}, {N_F5, Q}, {N_A5, Q}, {N_F5, Q}, {N_G4, Q}, {N_B4, Q}, {N_D5, H}, {REST, Q},
};

//  КАНАТ — марш, под который удобно долбить
static const Note T_TUG[] = {
  {N_C4, E}, {N_C4, E}, {N_G3, E}, {N_C4, E}, {N_Eb4, E}, {N_C4, E}, {N_G3, E}, {N_G3, E},
  {N_F4, E}, {N_F4, E}, {N_C4, E}, {N_F4, E}, {N_Eb4, E}, {N_D4, E}, {N_C4, Q},
};

//  СТРЕЛОК — тревожный космос
static const Note T_SHOOTER[] = {
  {N_A3, E}, {N_A3, E}, {N_C4, E}, {N_A3, E}, {N_E4, E}, {N_A3, E}, {N_G4, E}, {N_F4, E},
  {N_A3, E}, {N_A3, E}, {N_C4, E}, {N_A3, E}, {N_E4, E}, {N_D4, E}, {N_C4, E}, {N_B3, E},
};

//  ПОНГ — лёгкие прыжки
static const Note T_PONG[] = {
  {N_E5, E}, {REST, E}, {N_E5, E}, {REST, E}, {N_G5, E}, {REST, E}, {N_C5, Q},
  {N_D5, E}, {REST, E}, {N_D5, E}, {REST, E}, {N_F5, E}, {REST, E}, {N_B4, Q},
};

//  ДУЭЛЬ — низкий гул без ритма: по музыке момент вспышки не угадать
static const Note T_DUEL[] = {
  {N_A3, 900}, {N_Bb3, 900}, {N_A3, 900}, {N_G3, 900},
};

//  СПРИНТ — галоп
static const Note T_SPRINT[] = {
  {N_G4, S}, {N_G4, S}, {N_D5, E}, {N_G4, S}, {N_G4, S}, {N_E5, E},
  {N_G4, S}, {N_G4, S}, {N_F5, E}, {N_E5, E}, {N_D5, E}, {N_B4, E},
};

//  БОМБА — тиканье в басу, темп растёт с фитилём
static const Note T_BOMB[] = {
  {N_E3, E}, {REST, E}, {N_E3, E}, {REST, E}, {N_F3, E}, {REST, E}, {N_E3, E}, {REST, E},
  {N_G3, E}, {REST, E}, {N_F3, E}, {REST, E}, {N_E3, E}, {REST, E}, {N_Eb3, E}, {REST, E},
};

//  ОРБИТА — кружащее арпеджио, темп растёт со скоростью кометы
static const Note T_ORBIT[] = {
  {N_C4, E}, {N_E4, E}, {N_G4, E}, {N_C5, E}, {N_G4, E}, {N_E4, E},
  {N_D4, E}, {N_F4, E}, {N_A4, E}, {N_D5, E}, {N_A4, E}, {N_F4, E},
  {N_B3, E}, {N_D4, E}, {N_G4, E}, {N_B4, E},
};

//  СНАЙПЕР — редкие точки, не мешают целиться
static const Note T_SNIPER[] = {
  {N_A4, E}, {REST, E}, {REST, E}, {N_E5, E}, {REST, E}, {REST, E}, {N_A4, E}, {REST, E},
  {N_C5, E}, {REST, E}, {REST, E}, {N_E5, E}, {REST, E}, {REST, Q},
};

//  БЕГУН — бодрый бег
static const Note T_RUNNER[] = {
  {N_C5, E}, {N_C5, E}, {N_G4, E}, {N_C5, E}, {N_E5, E}, {N_D5, E}, {N_C5, E}, {N_G4, E},
  {N_A4, E}, {N_A4, E}, {N_E4, E}, {N_A4, E}, {N_C5, E}, {N_B4, E}, {N_A4, E}, {N_G4, E},
};

//  ЗАЩИТА — героическая
static const Note T_DEFENDER[] = {
  {N_G3, E}, {N_G3, E}, {N_G3, E}, {N_C4, Q}, {N_E4, E}, {N_G4, Q}, {N_E4, E}, {N_C4, E},
  {N_D4, E}, {N_F4, Q}, {N_D4, E}, {N_B3, E}, {N_G3, Q},
};

//  СТОП-СИГНАЛ — ожидание у светофора, ровное
static const Note T_TRAFFIC[] = {
  {N_C4, E}, {REST, E}, {N_C4, E}, {REST, E}, {REST, Q},
  {N_E4, E}, {REST, E}, {N_E4, E}, {REST, E}, {REST, Q},
};

//  БОЙ — агрессивная
static const Note T_FIGHT[] = {
  {N_D4, E}, {N_D4, E}, {N_F4, E}, {N_D4, E}, {N_A4, Q}, {N_G4, E}, {N_F4, E},
  {N_E4, E}, {N_D4, E}, {N_C4, E}, {N_D4, E}, {N_A3, Q}, {REST, Q},
};

//  ЖАДНОСТЬ — крадущаяся
static const Note T_GREED[] = {
  {N_E4, E}, {N_G4, E}, {N_Bb4, E}, {N_G4, E}, {N_E4, E}, {N_G4, E}, {N_Cs5, E}, {N_G4, E},
  {N_F4, E}, {N_A4, E}, {N_C5, E}, {N_A4, E}, {N_F4, E}, {N_A4, E}, {N_D5, E}, {N_A4, E},
};

//  ДОГОНЯЛКИ — суетливая
static const Note T_CHASE[] = {
  {N_A4, S}, {N_B4, S}, {N_C5, S}, {N_D5, S}, {N_E5, E}, {N_E5, E},
  {N_D5, S}, {N_C5, S}, {N_B4, S}, {N_A4, S}, {N_G4, E}, {N_G4, E},
};

//  КЛАД — загадочная
static const Note T_HUNT[] = {
  {N_D4, Q}, {N_F4, Q}, {N_A4, Q}, {REST, Q}, {N_C5, Q}, {N_A4, Q}, {N_F4, Q}, {REST, Q},
  {N_E4, Q}, {N_G4, Q}, {N_Bb4, Q}, {REST, Q}, {N_A4, H}, {REST, H},
};

//  БАШНЯ — ровная стройка
static const Note T_TOWER[] = {
  {N_C4, E}, {N_E4, E}, {N_G4, E}, {N_E4, E}, {N_D4, E}, {N_F4, E}, {N_A4, E}, {N_F4, E},
  {N_E4, E}, {N_G4, E}, {N_B4, E}, {N_G4, E}, {N_F4, E}, {N_A4, E}, {N_C5, E}, {N_A4, E},
};

//  ЗМЕЙКА — простая
static const Note T_SNAKE[] = {
  {N_C4, E}, {N_D4, E}, {N_E4, E}, {N_F4, E}, {N_G4, Q}, {N_F4, E}, {N_E4, E}, {N_D4, Q}, {REST, Q},
};

//  РИТМ — короткий отсчёт, чтобы не наезжать на первые ноты
static const Note T_RHYTHM[] = {
  {N_C5, E}, {N_E5, E}, {N_G5, E}, {N_C6, Q},
};

//  ОСАДА — мрачный марш
static const Note T_SIEGE[] = {
  {N_D3, Q}, {N_D3, E}, {N_D3, E}, {N_F3, Q}, {N_E3, Q}, {N_D3, Q}, {N_D3, E}, {N_D3, E},
  {N_G3, Q}, {N_F3, Q}, {N_A3, Q}, {N_G3, Q}, {N_F3, Q}, {N_E3, Q},
};

//  ЛОВИ — лёгкая, "падающая" вниз
static const Note T_CATCH[] = {
  {N_G5, E}, {N_E5, E}, {N_C5, E}, {N_G4, E}, {N_A4, E}, {N_C5, E}, {N_E5, Q},
  {N_F5, E}, {N_D5, E}, {N_B4, E}, {N_G4, E}, {N_C5, Q},
};

//  ПОНГ 2D — упругие интервалы
static const Note T_PONG2D[] = {
  {N_C5, E}, {N_G5, E}, {REST, E}, {N_E5, E}, {N_C6, E}, {REST, E},
  {N_A4, E}, {N_E5, E}, {REST, E}, {N_F5, E}, {N_D5, Q},
};

//  ЦВЕТА — головоломочная, с паузами
static const Note T_COLORS[] = {
  {N_E5, E}, {N_B4, E}, {N_C5, E}, {N_D5, E}, {N_C5, E}, {N_B4, E}, {N_A4, Q},
  {N_A4, E}, {N_C5, E}, {N_E5, Q}, {N_D5, E}, {N_C5, E}, {N_B4, Q},
};

//  ДИНО — топот по пустыне
static const Note T_DINO[] = {
  {N_D4, E}, {N_D4, E}, {N_F4, E}, {N_A4, E}, {N_G4, E}, {N_F4, E}, {N_D4, Q},
  {N_C4, E}, {N_C4, E}, {N_E4, E}, {N_G4, E}, {N_A4, E}, {N_G4, E}, {N_D4, Q},
};

const MusicTrack MUSIC_MENU     = makeTrack(T_MENU);
const MusicTrack MUSIC_TUG      = makeTrack(T_TUG);
const MusicTrack MUSIC_SHOOTER  = makeTrack(T_SHOOTER);
const MusicTrack MUSIC_PONG     = makeTrack(T_PONG);
const MusicTrack MUSIC_DUEL     = makeTrack(T_DUEL);
const MusicTrack MUSIC_SPRINT   = makeTrack(T_SPRINT);
const MusicTrack MUSIC_BOMB     = makeTrack(T_BOMB);
const MusicTrack MUSIC_ORBIT    = makeTrack(T_ORBIT);
const MusicTrack MUSIC_SNIPER   = makeTrack(T_SNIPER);
const MusicTrack MUSIC_RUNNER   = makeTrack(T_RUNNER);
const MusicTrack MUSIC_DEFENDER = makeTrack(T_DEFENDER);
const MusicTrack MUSIC_TRAFFIC  = makeTrack(T_TRAFFIC);
const MusicTrack MUSIC_FIGHT    = makeTrack(T_FIGHT);
const MusicTrack MUSIC_GREED    = makeTrack(T_GREED);
const MusicTrack MUSIC_CHASE    = makeTrack(T_CHASE);
const MusicTrack MUSIC_HUNT     = makeTrack(T_HUNT);
const MusicTrack MUSIC_TOWER    = makeTrack(T_TOWER);
const MusicTrack MUSIC_SNAKE    = makeTrack(T_SNAKE);
const MusicTrack MUSIC_RHYTHM   = makeTrack(T_RHYTHM);
const MusicTrack MUSIC_SIEGE    = makeTrack(T_SIEGE);
const MusicTrack MUSIC_CATCH    = makeTrack(T_CATCH);
const MusicTrack MUSIC_PONG2D   = makeTrack(T_PONG2D);
const MusicTrack MUSIC_COLORS   = makeTrack(T_COLORS);
const MusicTrack MUSIC_DINO     = makeTrack(T_DINO);

// ============================================================
//  Банки эффектов. Чего нет в банке — звучит стандартно.
// ============================================================

// СТРЕЛОК — лазеры и короткие хлопки
static const Note S_SHOOTER_HIT[]  = { {220, 60}, {160, 80} };
static const Note S_SHOOTER_EXPL[] = { {120, 50}, {90, 60}, {60, 120} };
static const SfxOverride B_SHOOTER[] = {
  SFX_SLIDE(Shoot, 2600, 1200, 50),
  SFX_SLIDE(ShootCharged, 3000, 600, 180),
  SFX_MEL(Hit, S_SHOOTER_HIT),
  SFX_MEL(Explode, S_SHOOTER_EXPL),
};

// ПОНГ — резиновый шпингалет и мягкая подача
static const Note S_PONG_SERVE[] = { {660, 40}, {880, 40} };
static const SfxOverride B_PONG[] = {
  SFX_SLIDE(Twang, 200, 1800, 90),
  SFX_MEL(Beep, S_PONG_SERVE),
};

// ДУЭЛЬ / СТОП-СИГНАЛ — сухой "щелчок затвора" на фальстарт
static const Note S_DUEL_ERR[] = { {260, 80}, {0, 40}, {220, 140} };
static const SfxOverride B_DUEL[] = {
  SFX_MEL(Error, S_DUEL_ERR),
};

// БОМБА — металлическое тиканье и глухой взрыв
static const Note S_BOMB_TICK[] = { {2600, 10} };
static const Note S_BOMB_EXPL[] = { {90, 80}, {70, 100}, {50, 200} };
static const Note S_BOMB_ERR[]  = { {150, 120} };
static const SfxOverride B_BOMB[] = {
  SFX_MEL(Tick, S_BOMB_TICK),
  SFX_MEL(Explode, S_BOMB_EXPL),
  SFX_MEL(Error, S_BOMB_ERR),
};

// ОРБИТА — звонкая подача, гулкий пролёт мимо
static const Note S_ORBIT_SERVE[] = { {523, 40}, {784, 40}, {1047, 60} };
static const Note S_ORBIT_EXPL[]  = { {300, 60}, {200, 80}, {120, 160} };
static const Note S_ORBIT_ERR[]   = { {200, 80}, {0, 30}, {180, 120} };
static const SfxOverride B_ORBIT[] = {
  SFX_MEL(Beep, S_ORBIT_SERVE),
  SFX_MEL(Explode, S_ORBIT_EXPL),
  SFX_MEL(Error, S_ORBIT_ERR),
};

// СНАЙПЕР — высокий "дзынь" попадания
static const Note S_SNIPER_PICK[] = { {1568, 40}, {2093, 60} };
static const Note S_SNIPER_LVL[]  = { {1047, 50}, {1319, 50}, {1568, 50}, {2093, 100} };
static const Note S_SNIPER_ERR[]  = { {240, 100} };
static const SfxOverride B_SNIPER[] = {
  SFX_MEL(Pickup, S_SNIPER_PICK),
  SFX_MEL(LevelUp, S_SNIPER_LVL),
  SFX_MEL(Error, S_SNIPER_ERR),
};

// БЕГУН — аркадная монетка
static const Note S_RUNNER_PICK[] = { {1319, 30}, {1760, 30}, {2093, 40} };
static const Note S_RUNNER_ERR[]  = { {330, 60}, {0, 30}, {262, 100} };
static const SfxOverride B_RUNNER[] = {
  SFX_MEL(Pickup, S_RUNNER_PICK),
  SFX_MEL(Error, S_RUNNER_ERR),
};

// ЗАЩИТА — тяжёлая пушка
static const Note S_DEF_HIT[]  = { {200, 40} };
static const Note S_DEF_EXPL[] = { {160, 60}, {110, 80}, {70, 140} };
static const Note S_DEF_LVL[]  = { {392, 70}, {523, 70}, {784, 140} };
static const SfxOverride B_DEFENDER[] = {
  SFX_SLIDE(Shoot, 900, 300, 60),
  SFX_MEL(Hit, S_DEF_HIT),
  SFX_MEL(Explode, S_DEF_EXPL),
  SFX_MEL(LevelUp, S_DEF_LVL),
};

// БОЙ — глухие удары, звон блока
static const Note S_FIGHT_HIT[]   = { {180, 60}, {140, 80} };
static const Note S_FIGHT_BLOCK[] = { {1200, 30}, {1600, 30} };
static const Note S_FIGHT_ERR[]   = { {200, 90}, {0, 40}, {160, 130} };
static const SfxOverride B_FIGHT[] = {
  SFX_MEL(Hit, S_FIGHT_HIT),
  SFX_MEL(Bounce, S_FIGHT_BLOCK),
  SFX_MEL(Error, S_FIGHT_ERR),
};

// ЖАДНОСТЬ — звон монет в банк, мина
static const Note S_GREED_PICK[] = { {784, 40}, {988, 40}, {1319, 60} };
static const Note S_GREED_EXPL[] = { {140, 60}, {100, 80}, {70, 160} };
static const SfxOverride B_GREED[] = {
  SFX_MEL(Pickup, S_GREED_PICK),
  SFX_MEL(Explode, S_GREED_EXPL),
};

// КЛАД — приглушённый "не твой ход"
static const Note S_HUNT_ERR[] = { {300, 70} };
static const SfxOverride B_HUNT[] = {
  SFX_MEL(Error, S_HUNT_ERR),
};

// БАШНЯ — звон идеального этажа, обвал
static const Note S_TOWER_LVL[]  = { {1047, 60}, {1319, 60}, {1568, 120} };
static const Note S_TOWER_OVER[] = { {330, 120}, {262, 140}, {196, 300} };
static const SfxOverride B_TOWER[] = {
  SFX_MEL(LevelUp, S_TOWER_LVL),
  SFX_MEL(GameOver, S_TOWER_OVER),
};

// ЗМЕЙКА — короткий укус
static const Note S_SNAKE_PICK[] = { {880, 30}, {1175, 40} };
static const SfxOverride B_SNAKE[] = {
  SFX_MEL(Pickup, S_SNAKE_PICK),
};

// РИТМ — ошибка глухая, чтобы не сбивать с такта
static const Note S_RHYTHM_ERR[] = { {196, 80}, {0, 30}, {175, 120} };
static const Note S_RHYTHM_LVL[] = { {784, 60}, {1047, 60}, {1319, 120} };
static const SfxOverride B_RHYTHM[] = {
  SFX_MEL(Error, S_RHYTHM_ERR),
  SFX_MEL(LevelUp, S_RHYTHM_LVL),
};

// ОСАДА — низкие орудия
static const Note S_SIEGE_HIT[]  = { {180, 50} };
static const Note S_SIEGE_EXPL[] = { {100, 70}, {80, 90}, {55, 180} };
static const Note S_SIEGE_LVL[]  = { {294, 80}, {392, 80}, {587, 160} };
static const SfxOverride B_SIEGE[] = {
  SFX_SLIDE(Shoot, 700, 250, 70),
  SFX_MEL(Hit, S_SIEGE_HIT),
  SFX_MEL(Explode, S_SIEGE_EXPL),
  SFX_MEL(LevelUp, S_SIEGE_LVL),
};

// ЛОВИ — поймал: короткий "хоп", упустил: шлепок
static const Note S_CATCH_PICK[] = { {1047, 25}, {1568, 45} };
static const Note S_CATCH_ERR[]  = { {220, 70}, {0, 30}, {165, 110} };
static const Note S_CATCH_CLICK[] = { {1400, 12} };
static const SfxOverride B_CATCH[] = {
  SFX_MEL(Pickup, S_CATCH_PICK),
  SFX_MEL(Error, S_CATCH_ERR),
  SFX_MEL(Click, S_CATCH_CLICK),
};

// ПОНГ 2D — стенка глухая, подача звонкая
static const Note S_P2D_BOUNCE[] = { {440, 20} };
static const Note S_P2D_SERVE[]  = { {784, 35}, {1175, 50} };
static const SfxOverride B_PONG2D[] = {
  SFX_MEL(Bounce, S_P2D_BOUNCE),
  SFX_MEL(Beep, S_P2D_SERVE),
};

// ЦВЕТА — глухое приземление, звон линии, фанфара цепочки
static const Note S_COL_LAND[]  = { {330, 18} };
static const Note S_COL_MATCH[] = { {1047, 40}, {1319, 40}, {1568, 70} };
static const Note S_COL_CHAIN[] = { {1047, 40}, {1319, 40}, {1568, 40}, {2093, 120} };
static const Note S_COL_MOVE[]  = { {1200, 10} };
static const SfxOverride B_COLORS[] = {
  SFX_MEL(Tick, S_COL_LAND),
  SFX_MEL(Pickup, S_COL_MATCH),
  SFX_MEL(LevelUp, S_COL_CHAIN),
  SFX_MEL(Click, S_COL_MOVE),
};

// ДИНО — шаг в сторону глухой, удар о камень, "ух" каждые 5 обойдённых
static const Note S_DINO_STEP[] = { {300, 10} };
static const Note S_DINO_HIT[]  = { {180, 60}, {0, 20}, {120, 120} };
static const Note S_DINO_UH[]   = { {880, 30}, {1175, 50} };
static const SfxOverride B_DINO[] = {
  SFX_MEL(Click, S_DINO_STEP),
  SFX_MEL(Error, S_DINO_HIT),
  SFX_MEL(Pickup, S_DINO_UH),
};

const SfxBank SND_SHOOTER  = makeBank(B_SHOOTER);
const SfxBank SND_PONG     = makeBank(B_PONG);
const SfxBank SND_DUEL     = makeBank(B_DUEL);
const SfxBank SND_BOMB     = makeBank(B_BOMB);
const SfxBank SND_ORBIT    = makeBank(B_ORBIT);
const SfxBank SND_SNIPER   = makeBank(B_SNIPER);
const SfxBank SND_RUNNER   = makeBank(B_RUNNER);
const SfxBank SND_DEFENDER = makeBank(B_DEFENDER);
const SfxBank SND_FIGHT    = makeBank(B_FIGHT);
const SfxBank SND_GREED    = makeBank(B_GREED);
const SfxBank SND_HUNT     = makeBank(B_HUNT);
const SfxBank SND_TOWER    = makeBank(B_TOWER);
const SfxBank SND_SNAKE    = makeBank(B_SNAKE);
const SfxBank SND_RHYTHM   = makeBank(B_RHYTHM);
const SfxBank SND_SIEGE    = makeBank(B_SIEGE);
const SfxBank SND_CATCH    = makeBank(B_CATCH);
const SfxBank SND_PONG2D   = makeBank(B_PONG2D);
const SfxBank SND_COLORS   = makeBank(B_COLORS);
const SfxBank SND_DINO     = makeBank(B_DINO);

#pragma once
#include "game.h"

// ============================================================
//  1) TUG OF WAR — перетягивание каната (2 игрока)
//     Оба долбят свою кнопку, узел каната ползёт. Кто вытолкнул
//     узел за пределы средней зоны [MIDDLE_LO..MIDDLE_HI] — забрал
//     раунд. Частые нажатия дают комбо-множитель.
// ============================================================
class TugGame : public Game {
public:
  const char* name() const override { return "КАНАТ"; }
  const char* hint() const override { return "ДОЛБИ! ВЫШИБИ ИЗ ЗОНЫ"; }
  Color theme() const override { return rgb(255, 120, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ЖМИ КНОПКУ ЧАЩЕ",
      "ТЯНЕШЬ УЗЕЛ К СЕБЕ",
      "ЧАСТО ЖМИ = КОМБО",
      "ВЫШИБИ УЗЕЛ ЗА КРАЙ",
      "СРЕДНЕЙ ЗОНЫ",
      "ЭТО ПОБЕДА В РАУНДЕ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match    m;
  float    rope = LED_COUNT / 2.0f;
  uint32_t lastPress[2] = {0, 0};
  uint8_t  combo[2] = {1, 1};
  float    heat[2] = {0, 0};      // визуальный "накал" стороны

  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  2) SHOOTER — стрелялка (2 игрока)
//     У каждого база из HP светодиодов. Тап — обычный снаряд,
//     удержание — заряженный (пробивает один вражеский).
// ============================================================
struct Shot {
  float   pos = 0;
  float   vel = 0;
  int8_t  owner = 0;     // 1 или 2
  uint8_t power = 1;     // 1 обычный, 2 заряженный
  bool    active = false;
};

class ShooterGame : public Game {
public:
  const char* name() const override { return "СТРЕЛОК"; }
  const char* hint() const override { return "ТАП=ОГОНЬ УДЕРЖ=ЗАРЯД"; }
  Color theme() const override { return rgb(255, 40, 40); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ТАП = ВЫСТРЕЛ",
      "УДЕРЖИ = ЗАРЯД",
      "ЗАРЯД ПРОБЬЕТ ВРАГА",
      "СБЕЙ ВСЕ ЖИЗНИ ВРАГА",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match    m;
  int      hp[2] = {SHOOTER_HP, SHOOTER_HP};
  Shot     shots[SHOT_MAX];
  uint32_t cd[2] = {0, 0};
  bool     chargeAnnounced[2] = {false, false};

  void newRound();
  void fire(int player, bool charged, Ctx& c);
  void render(Ctx& c, const Inputs& in);
  void oled(Ctx& c, const Inputs& in);
};

// ============================================================
//  3) PONG — одномерный пинг-понг со шпингалетом (2 игрока)
//     Держишь кнопку — шпингалет оттягивается и копит энергию.
//     Отпустил — резко выстреливает вперёд. Попал по мячу во время
//     выброса — отбил, и чем больше был заряд, тем быстрее мяч.
// ============================================================
enum class PaddlePhase : uint8_t { Idle, Charging, Strike, Return };

struct Paddle {
  PaddlePhase phase = PaddlePhase::Idle;
  float charge = 0;      // накопленная энергия, пикселей
  float ext = 0;         // текущий вылет
  float lastStrike = 0;  // с каким зарядом ушёл последний удар

  void reset();
  void update(float dt, bool held, bool released);
  bool striking() const { return phase == PaddlePhase::Strike; }
};

class PongGame : public Game {
public:
  const char* name() const override { return "ПОНГ"; }
  const char* hint() const override { return "ДЕРЖИ ЗАРЯД, ОТПУСТИ"; }
  Color theme() const override { return rgb(0, 220, 220); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ДЕРЖИ = ЗАРЯД",
      "ОТПУСТИ = УДАР",
      "ПОПАДИ ПО МЯЧУ",
      "ЗАРЯД - СКОРОСТЬ",
      "ПРОПУСК = ОЧКО ВРАГУ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match  m;
  Paddle pad[2];
  float  ball = LED_COUNT / 2.0f;
  float  vel = 0;         // знак = направление
  int    rally = 0;
  uint32_t serveAt = 0;   // до этого момента мяч висит в центре
  int    lastStep[2] = {-1, -1};   // для "ступенчатого" звука зарядки

  void newRally(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  4) DUEL — дуэль на реакцию (2 игрока)
//     Лента наливается красным, потом внезапно вспыхивает белым.
//     Кто первый нажал — забрал раунд. Нажал раньше сигнала —
//     фальстарт, раунд уходит сопернику.
// ============================================================
class DuelGame : public Game {
public:
  const char* name() const override { return "ДУЭЛЬ"; }
  const char* hint() const override { return "ЖДИ ВСПЫШКУ!"; }
  Color theme() const override { return rgb(255, 255, 255); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ЖДИ ВСПЫШКУ",
      "ЖМИ БЫСТРЕЕ ВСЕХ",
      "РАНЬШЕ = ПРОИГРЫШ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  enum class Ph : uint8_t { Arming, Wait, Go, Show };
  Match    m;
  Ph       ph = Ph::Arming;
  uint32_t phaseUntil = 0;
  uint32_t goAt = 0;
  int      rt[2] = {-1, -1};      // время реакции, мс
  bool     falseStart = false;

  void arm(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  5) SPRINT — гонка по ленте (2 игрока)
//     Бегун P1 стартует слева, P2 справа. Нажатие даёт импульс,
//     скорость гаснет. Кто первым добежал до чужого края — победа.
// ============================================================
class SprintGame : public Game {
public:
  const char* name() const override { return "СПРИНТ"; }
  const char* hint() const override { return "БЕГИ ДО КРАЯ"; }
  Color theme() const override { return rgb(255, 220, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ЖМИ ДЛЯ РЫВКА",
      "РИТМ ВАЖНЕЕ СИЛЫ",
      "ДОБЕГИ ПЕРВЫМ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match m;
  float pos[2] = {0, LED_COUNT - 1.0f};
  float vel[2] = {0, 0};
  uint32_t startAt = 0;   // до этого момента — обратный отсчёт

  void newRound(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  6) BOMB — горячая картошка (2 игрока)
//     Бомба летает по ленте. Пока она в твоей зоне (крайние
//     BOMB_ZONE пикселей) — жми, чтобы отбить обратно, каждый раз
//     быстрее. Улетела за твой край или взорвалась на твоей
//     половине — раунд проигран.
// ============================================================
class BombGame : public Game {
public:
  const char* name() const override { return "БОМБА"; }
  const char* hint() const override { return "ОТБЕЙ В СВОЕЙ ЗОНЕ"; }
  Color theme() const override { return rgb(255, 60, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ОТБЕЙ БОМБУ В ЗОНЕ",
      "КАЖДЫЙ ОТБОЙ БЫСТРЕЕ",
      "ПРОПУСК ИЛИ ВЗРЫВ",
      "У ТЕБЯ = ПРОИГРЫШ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match    m;
  float    pos = LED_COUNT / 2.0f;
  float    vel = BOMB_SPEED_START;
  uint32_t fuseEnd = 0;
  uint32_t fuseTotal = 1;
  uint32_t nextTick = 0;
  uint32_t stun[2] = {0, 0};
  int      volleys = 0;

  void newRound(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  7) SNIPER — меткость (1 игрок)
//     Курсор мечется по ленте, светится зона-мишень. Жми ровно
//     когда курсор внутри. Попал — зона уже, курсор быстрее.
// ============================================================
class SniperGame : public Game {
public:
  const char* name() const override { return "СНАЙПЕР"; }
  const char* hint() const override { return "ЗЕЛЕНАЯ: СТОП В ЗОНЕ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(255, 0, 120); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "КУРСОР БЕГАЕТ",
      "ЖМИ ЗЕЛЕНУЮ В ЗОНЕ",
      "ЦЕНТР ЗОНЫ = ОЧКИ",
      "ЗОНА СУЖАЕТСЯ",
      "3 ПРОМАХА = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  float    cursor = 0;
  float    speed = SNIPER_SPEED_START;
  int      dir = 1;
  int      zoneLo = 0, zoneHi = 0;
  int      zoneW = SNIPER_ZONE_START;
  int      lives = SNIPER_LIVES;
  int      score = 0;
  int      level = 1;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;
  uint32_t hitUntil = 0;
  bool     hitGood = false;

  void placeZone();
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  8) SIMON — повтори последовательность (1 игрок)
//     Лента показывает серию вспышек: зелёная (левая половина) или
//     синяя (правая). Повторяй кнопками в том же порядке.
// ============================================================
class SimonGame : public Game {
public:
  const char* name() const override { return "САЙМОН"; }
  const char* hint() const override { return "ПОВТОРИ СЕРИЮ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(160, 0, 255); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "СМОТРИ ВСПЫШКИ",
      "ПОВТОРИ КНОПКАМИ",
      "ЛЕВО = ЗЕЛЕНАЯ",
      "ПРАВО = СИНЯЯ",
      "ОШИБКА = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  enum class Ph : uint8_t { Intro, Show, Gap, Input, Good, Bad, Over };
  uint8_t  seq[SIMON_MAX_LEN];
  int      len = 0;
  int      showIdx = 0;
  int      inputIdx = 0;
  Ph       ph = Ph::Intro;
  uint32_t phaseUntil = 0;
  uint16_t showMs = 480;
  int      best = 0;
  bool     newRecord = false;
  int      flashSide = -1;      // что подсвечивать сейчас: 0 лево, 1 право
  uint32_t flashUntil = 0;      // до какого момента держать подсветку

  void nextLevel(Ctx& c);
  void startShow(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  9) RUNNER — бегун с препятствиями (1 игрок)
//     Игрок стоит на позиции RUNNER_POS. Навстречу летят красные
//     (прыгай — ЗЕЛЁНАЯ) и синие (подкат — СИНЯЯ) препятствия.
// ============================================================
struct Obstacle {
  float pos = 0;
  uint8_t kind = 0;    // 0 = красное (прыжок), 1 = синее (подкат)
  bool active = false;
  bool judged = false;
};

class RunnerGame : public Game {
public:
  const char* name() const override { return "БЕГУН"; }
  const char* hint() const override { return "КР=ПРЫЖОК СИН=ПОДКАТ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(0, 255, 160); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "КРАСНОЕ = ПРЫЖОК",
      "СИНЕЕ = ПОДКАТ",
      "ЗЕЛЕНАЯ ИЛИ СИНЯЯ",
      "ВОВРЕМЯ = ОЧКО",
      "3 ОШИБКИ = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  static const int MAX_OBS = 6;
  Obstacle obs[MAX_OBS];
  float    speed = 9.0f;
  float    spawnIn = 1.2f;
  int      lives = RUNNER_LIVES;
  int      score = 0;
  int      best = 0;
  bool     newRecord = false;
  uint8_t  action = 0;            // 0 нет, 1 прыжок, 2 подкат
  uint32_t actionUntil = 0;
  uint32_t hurtUntil = 0;
  bool     over = false;
  uint32_t overUntil = 0;

  void spawn();
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  10) DEFENDER — оборона базы (1 игрок)
//      База слева. Справа ползут враги. ЗЕЛЁНАЯ — выстрел,
//      СИНЯЯ — импульс, сметающий всё рядом (ограниченный запас).
// ============================================================
struct Enemy {
  float   pos = 0;
  float   vel = 0;
  int8_t  hp = 0;
  uint8_t kind = 0;    // 0 обычный, 1 бронированный, 2 босс
  bool    active = false;
  uint32_t flashUntil = 0;
};

struct Bullet {
  float pos = 0;
  bool  active = false;
};

class DefenderGame : public Game {
public:
  const char* name() const override { return "ЗАЩИТА"; }
  const char* hint() const override { return "З=ВЫСТРЕЛ С=ВЗРЫВ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(0, 160, 255); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "ЗЕЛЕНАЯ = ВЫСТРЕЛ",
      "СИНЯЯ = ВЗРЫВ",
      "ВЗРЫВ ОГРАНИЧЕН",
      "НЕ ПУСКАЙ ВРАГОВ",
      "К БАЗЕ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  static const int MAX_BULLETS = 6;
  Enemy    enemies[DEFENDER_MAX_ENEMY];
  Bullet   bullets[MAX_BULLETS];
  int      lives = DEFENDER_LIVES;
  int      score = 0;
  int      wave = 1;
  int      killsInWave = 0;
  int      nukes = DEFENDER_NUKES;
  float    spawnIn = 1.0f;
  uint32_t cd = 0;
  uint32_t nukeFlashUntil = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;

  void spawnEnemy();
  void shoot(Ctx& c);
  void nuke(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  11) TRAFFIC — стоп-сигнал (2 игрока)
//      Лента вспыхивает зелёным (жми!) или красным (не смей).
//      Реакция против выдержки: главный враг — собственный палец.
// ============================================================
class TrafficGame : public Game {
public:
  const char* name() const override { return "СТОП-СИГНАЛ"; }
  const char* hint() const override { return "ЗЕЛЕНОЕ ЖМИ, КРАСНОЕ НЕТ"; }
  Color theme() const override { return rgb(0, 255, 40); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ЗЕЛЕНАЯ ВСПЫШКА -",
      "ЖМИ БЫСТРЕЕ ВРАГА",
      "КРАСНАЯ ВСПЫШКА -",
      "НЕ ЖМИ ВООБЩЕ",
      "НАЖАЛ = ОЧКО ВРАГУ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  enum class Ph : uint8_t { Arming, Wait, Signal, Judged };
  Match    m;
  Ph       ph = Ph::Arming;
  uint32_t phaseUntil = 0;
  uint32_t signalAt = 0;
  bool     redLight = false;     // true = жать нельзя
  int      rt[2] = {-1, -1};     // время реакции, мс (-2 = ошибка)

  void arm(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  12) FIGHT — бой на двоих (2 игрока)
//      Тап = удар, удержание = блок. Блок жрёт стамину, удар в
//      блок отбрасывает атакующего. Первая игра "на обман".
// ============================================================
class FightGame : public Game {
public:
  const char* name() const override { return "БОЙ"; }
  const char* hint() const override { return "ТАП=УДАР ДЕРЖИ=БЛОК"; }
  Color theme() const override { return rgb(255, 80, 140); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ТАП = УДАР",
      "ДЕРЖИ = БЛОК",
      "БЛОК ТРАТИТ СИЛУ",
      "УДАР В БЛОК =",
      "СТУПОР У БЬЮЩЕГО",
      "СИЛА КОНЧИЛАСЬ -",
      "БЛОК ЛОМАЕТСЯ",
      "СНИМИ ВСЕ ЖИЗНИ",
      "МАТЧ ДО 3 ПОБЕД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  struct Strike {
    uint32_t start = 0;     // когда вышел удар
    int8_t   owner = 0;     // 1 или 2
    bool     active = false;
  };

  Match    m;
  int      hp[2] = {FIGHT_HP, FIGHT_HP};
  float    stam[2] = {FIGHT_STAMINA, FIGHT_STAMINA};
  bool     guard[2] = {false, false};
  uint32_t stunUntil[2] = {0, 0};
  uint32_t cd[2] = {0, 0};
  uint32_t hurtUntil[2] = {0, 0};
  Strike   strikes[4];
  const char* event = "";

  void newRound();
  void launch(int p, Ctx& c);
  void resolve(int idx, Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  13) GREED — жадность (2 игрока)
//      Держишь кнопку — полоска ползёт от твоего края и копит
//      очки. Где-то на ней спрятана мина. Отпустил вовремя —
//      забрал банк, дожал — сгорело всё.
// ============================================================
class GreedGame : public Game {
public:
  const char* name() const override { return "ЖАДНОСТЬ"; }
  const char* hint() const override { return "ДЕРЖИ И ВОВРЕМЯ БРОСЬ"; }
  Color theme() const override { return rgb(255, 190, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ДЕРЖИ КНОПКУ -",
      "ПОЛОСКА РАСТЕТ",
      "ДЛИННЕЕ = БОЛЬШЕ ОЧКОВ",
      "ГДЕ-ТО СПРЯТАНА МИНА",
      "ОТПУСТИЛ = В БАНК",
      "ДОЖАЛ ДО МИНЫ = НОЛЬ",
      "БАНК БОЛЬШЕ = РАУНД ТВОЙ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match    m;
  float    len[2] = {0, 0};       // текущая длина полоски
  int      mine[2] = {0, 0};      // где мина (в пикселях от края)
  int      bank[2] = {0, 0};      // забранные очки
  bool     done[2] = {false, false};
  bool     bust[2] = {false, false};
  uint32_t roundEnd = 0;
  uint32_t nextTick[2] = {0, 0};
  const char* event = "";

  void newRound(uint32_t now);
  void finish(int p, bool exploded, Ctx& c);
  void resolveRound(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  14) CHASE — догонялки по кольцу (2 игрока)
//      Лента считается замкнутой. Один убегает, другой догоняет,
//      оба долбят кнопку. Роли меняются каждый раунд.
// ============================================================
class ChaseGame : public Game {
public:
  const char* name() const override { return "ДОГОНЯЛКИ"; }
  const char* hint() const override { return "ЛЕНТА ЗАМКНУТА В КОЛЬЦО"; }
  Color theme() const override { return rgb(255, 0, 200); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "ЛЕНТА = КОЛЬЦО",
      "ЖМИ ЧТОБЫ БЕЖАТЬ",
      "ОХОТНИК ЧУТЬ БЫСТРЕЕ",
      "ДОГНАЛ ВОВРЕМЯ -",
      "ОЧКО ОХОТНИКУ",
      "УБЕЖАЛ - ОЧКО БЕГЛЕЦУ",
      "РОЛИ МЕНЯЮТСЯ КАЖДЫЙ РАУНД",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  Match    m;
  float    pos[2] = {0, 0};
  float    vel[2] = {0, 0};
  int      hunter = 2;            // кто догоняет в этом раунде (1 или 2)
  uint32_t startAt = 0;
  uint32_t endAt = 0;
  float    gap = 0;               // текущая дистанция по кольцу

  void newRound(uint32_t now);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  15) HUNT — клад (2 игрока)
//      Где-то на ленте зарыт клад. Ходы по очереди: курсор бежит,
//      тап = раскопка. Лента и бузер отвечают "горячо/холодно",
//      отметки остаются всем видны — кто быстрее сообразит.
// ============================================================
class HuntGame : public Game {
public:
  const char* name() const override { return "КЛАД"; }
  const char* hint() const override { return "ХОДИМ ПО ОЧЕРЕДИ"; }
  Color theme() const override { return rgb(255, 120, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "2 ИГРОКА",
      "КЛАД СПРЯТАН НА ЛЕНТЕ",
      "ХОДЫ ПО ОЧЕРЕДИ",
      "ТАП = КОПАТЬ ТУТ",
      "ЦВЕТ И ЗВУК СКАЖУТ",
      "ГОРЯЧО ИЛИ ХОЛОДНО",
      "ОТМЕТКИ ВИДНЫ ОБОИМ",
      "НАШЕЛ = РАУНД ТВОЙ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  struct Mark { int8_t pos = -1; uint8_t dist = 0; int8_t owner = 0; };

  Match    m;
  int      treasure = 0;
  float    cursor = 0;
  float    speed = HUNT_SPEED_START;
  int      dir = 1;
  int      turn = 1;              // чей ход
  Mark     marks[HUNT_MAX_MARKS];
  uint8_t  markCount = 0;
  int      shots[2] = {0, 0};
  int      lastDist[2] = {-1, -1};
  uint32_t digUntil = 0;
  int      digPos = -1;
  const char* event = "";

  void newRound(uint32_t now);
  void dig(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  16) TOWER — башня (1 игрок)
//      Блок ездит по ленте, тап фиксирует этаж. Что не совпало
//      с предыдущим — отваливается, блок становится короче.
// ============================================================
class TowerGame : public Game {
public:
  const char* name() const override { return "БАШНЯ"; }
  const char* hint() const override { return "ЖМИ РОВНО НАД ЭТАЖОМ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(0, 200, 255); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "БЛОК ЕЗДИТ ПО ЛЕНТЕ",
      "ЗЕЛЕНАЯ = ПОСТАВИТЬ",
      "ЛИШНЕЕ ОТВАЛИВАЕТСЯ",
      "БЛОК СТАНОВИТСЯ УЖЕ",
      "МИМО ЭТАЖА = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  float    pos = 0;
  float    speed = TOWER_SPEED_START;
  int      dir = 1;
  int      width = TOWER_WIDTH_START;
  int      prevLo = 0, prevHi = LED_COUNT - 1;
  int      level = 1;
  int      score = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;
  uint32_t lockUntil = 0;

  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  17) SNAKE — змейка по кольцу (1 игрок)
//      Змея ползёт по замкнутой ленте. ЗЕЛЁНАЯ разворачивает её
//      (хвост становится головой), СИНЯЯ ускоряет. Каждая еда
//      добавляет камень — кольцо становится теснее.
// ============================================================
class SnakeGame : public Game {
public:
  const char* name() const override { return "ЗМЕЙКА"; }
  const char* hint() const override { return "ЗЕЛ=РАЗВОРОТ СИН=РЫВОК"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(60, 255, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "ЛЕНТА = КОЛЬЦО",
      "ЗЕЛЕНАЯ = РАЗВОРОТ",
      "СИНЯЯ = УСКОРЕНИЕ",
      "ЕШЬ ЖЕЛТУЮ ЕДУ",
      "ПОСЛЕ ЕДЫ РАСТЕТ КАМЕНЬ",
      "КАМЕНЬ ИЛИ ХВОСТ =",
      "КОНЕЦ ИГРЫ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  int8_t   body[SNAKE_MAX_LEN];   // body[0] — голова
  int      len = SNAKE_START_LEN;
  int      dir = 1;
  bool     rock[LED_COUNT];
  int      food = 0;
  uint16_t stepMs = SNAKE_STEP_START;
  uint32_t nextStep = 0;
  int      score = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;
  uint32_t flipUntil = 0;

  bool occupied(int p) const;
  int  arcCells(int* out, int cap) const;   // дуга, доступная змее
  void placeFood();
  void placeRock();
  void step(Ctx& c);
  void die(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  18) RHYTHM — ритм (1 игрок)
//      Ноты едут к наковальне у левого края под тиканье бузера.
//      Жми ровно в долю: точность важнее скорости.
// ============================================================
class RhythmGame : public Game {
public:
  const char* name() const override { return "РИТМ"; }
  const char* hint() const override { return "ЖМИ В ДОЛЮ"; }
  uint8_t players() const override { return 1; }
  Color theme() const override { return rgb(255, 0, 90); }
  RuleText rules() const override {
    static const char* L[] = {
      "1 ИГРОК",
      "НОТЫ ЕДУТ К КРАЮ",
      "ЖМИ ЗЕЛЕНУЮ КОГДА",
      "НОТА НА НАКОВАЛЬНЕ",
      "ТОЧНО В ЦЕЛЬ = БОЛЬШЕ",
      "ТЕМП РАСТЕТ",
      "3 ПРОМАХА = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  struct NoteHit { float pos = 0; bool active = false; };

  NoteHit  notes[RHYTHM_MAX_NOTES];
  uint16_t beatMs = RHYTHM_BEAT_START;
  uint32_t nextBeat = 0;
  float    speed = 0;
  int      lives = RHYTHM_LIVES;
  int      score = 0;
  int      combo = 0;
  int      bestCombo = 0;
  int      hits = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;
  uint32_t judgeUntil = 0;
  uint8_t  judge = 0;             // 0 нет, 1 точно, 2 хорошо, 3 мимо

  void spawn();
  void miss(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  19) SYNC — синхрон (кооператив)
//      Два импульса летят навстречу друг другу. Оба игрока
//      должны нажать ровно в момент встречи в центре.
//      Общий счёт, общий рекорд, окно допуска сужается.
// ============================================================
class SyncGame : public Game {
public:
  const char* name() const override { return "СИНХРОН"; }
  const char* hint() const override { return "ЖМИТЕ ОБА В ЦЕНТРЕ"; }
  bool coop() const override { return true; }
  Color theme() const override { return rgb(0, 255, 220); }
  RuleText rules() const override {
    static const char* L[] = {
      "КООПЕРАТИВ НА ДВОИХ",
      "ДВА ОГНЯ ЛЕТЯТ",
      "НАВСТРЕЧУ ДРУГ ДРУГУ",
      "ЖМИТЕ ОБА В МОМЕНТ",
      "ВСТРЕЧИ В ЦЕНТРЕ",
      "ОКНО СУЖАЕТСЯ С УРОВНЕМ",
      "3 ОШИБКИ = КОНЕЦ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  enum class Ph : uint8_t { Ready, Fly, Judge };
  Ph       ph = Ph::Ready;
  uint32_t phaseUntil = 0;
  uint32_t startAt = 0;
  uint32_t meetAt = 0;
  int      err[2] = {-1, -1};     // ошибка каждого игрока, мс (-1 = не нажал)
  bool     fired[2] = {false, false};
  uint16_t tol = SYNC_TOL_START;
  int      level = 1;
  int      lives = SYNC_LIVES;
  int      score = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     lastOk = false;
  bool     over = false;
  uint32_t overUntil = 0;
  const char* event = "";

  void arm(uint32_t now);
  void resolve(Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  20) SIEGE — осада (кооператив)
//      База в центре ленты, враги лезут с обоих краёв. Зелёный
//      держит левый фланг, синий — правый. Жизни и счёт общие.
// ============================================================
class SiegeGame : public Game {
public:
  const char* name() const override { return "ОСАДА"; }
  const char* hint() const override { return "ДЕРЖИ СВОЙ ФЛАНГ"; }
  bool coop() const override { return true; }
  Color theme() const override { return rgb(255, 60, 0); }
  RuleText rules() const override {
    static const char* L[] = {
      "КООПЕРАТИВ НА ДВОИХ",
      "БАЗА В ЦЕНТРЕ ЛЕНТЫ",
      "ВРАГИ ЛЕЗУТ С КРАЕВ",
      "ЗЕЛЕНАЯ БЬЕТ ВЛЕВО",
      "СИНЯЯ БЬЕТ ВПРАВО",
      "ЖИЗНИ И СЧЕТ ОБЩИЕ",
      "ВОЛНЫ СТАНОВЯТСЯ ЗЛЕЕ",
    };
    return { L, (uint8_t)(sizeof(L) / sizeof(L[0])) };
  }
  void reset() override;
  void update(const Inputs& in, Ctx& c) override;

private:
  struct Foe {
    float    pos = 0;
    float    vel = 0;
    int8_t   hp = 0;
    uint8_t  kind = 0;      // 0 обычный, 1 бронированный, 2 босс
    int8_t   side = -1;     // -1 слева, +1 справа
    bool     active = false;
    uint32_t flashUntil = 0;
  };
  struct Slug { float pos = 0; int8_t dir = 0; bool active = false; };

  Foe      foes[SIEGE_MAX_ENEMY];
  Slug     shots[SIEGE_MAX_BULLETS];
  int      lives = SIEGE_LIVES;
  int      score = 0;
  int      wave = 1;
  int      kills = 0;
  float    spawnIn = 1.0f;
  uint32_t cd[2] = {0, 0};
  uint32_t hurtUntil = 0;
  int      best = 0;
  bool     newRecord = false;
  bool     over = false;
  uint32_t overUntil = 0;

  void spawnFoe();
  void shoot(int p, Ctx& c);
  void render(Ctx& c);
  void oled(Ctx& c);
};

// ============================================================
//  Менеджер игр
// ============================================================
class GameManager {
public:
  static const int COUNT = 20;

  GameManager();

  int   count() const { return COUNT; }
  Game* at(int i) { return _games[i]; }
  Game* now() { return _games[_cur]; }
  int   index() const { return _cur; }

  void select(int i);
  void next();
  void prev();
  void resetCurrent();

  const char* const* names() const { return _names; }
  const uint8_t*     playerCounts() const { return _players; }

private:
  TugGame      _tug;
  ShooterGame  _shooter;
  PongGame     _pong;
  DuelGame     _duel;
  SprintGame   _sprint;
  BombGame     _bomb;
  SniperGame   _sniper;
  SimonGame    _simon;
  RunnerGame   _runner;
  DefenderGame _defender;
  TrafficGame  _traffic;
  FightGame    _fight;
  GreedGame    _greed;
  ChaseGame    _chase;
  HuntGame     _hunt;
  TowerGame    _tower;
  SnakeGame    _snake;
  RhythmGame   _rhythm;
  SyncGame     _sync;
  SiegeGame    _siege;

  Game*       _games[COUNT];
  const char* _names[COUNT];
  uint8_t     _players[COUNT];
  int         _cur = 0;
};

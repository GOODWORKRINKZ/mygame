#include "games.h"

// ============================================================
//  GameManager — список игр и переключение между ними
// ============================================================

GameManager::GameManager() {
  _games[0] = &_tug;
  _games[1] = &_shooter;
  _games[2] = &_pong;
  _games[3] = &_pong2d;
  _games[4] = &_duel;
  _games[5] = &_sprint;
  _games[6] = &_bomb;
  _games[7] = &_orbit;
  _games[8] = &_sniper;
  _games[9] = &_catch;
  _games[10] = &_colors;
  _games[11] = &_dino;
  _games[12] = &_simon;
  _games[13] = &_runner;
  _games[14] = &_defender;
  _games[15] = &_traffic;
  _games[16] = &_fight;
  _games[17] = &_greed;
  _games[18] = &_chase;
  _games[19] = &_hunt;
  _games[20] = &_tower;
  _games[21] = &_snake;
  _games[22] = &_rhythm;
  _games[23] = &_sync;
  _games[24] = &_siege;

  for (int i = 0; i < COUNT; i++) {
    _names[i] = _games[i]->name();
    // 3 — служебное значение "кооператив", меню рисует его как "КО"
    _players[i] = _games[i]->coop() ? 3 : _games[i]->players();
  }
}

void GameManager::select(int i) {
  if (i < 0) i = COUNT - 1;
  if (i >= COUNT) i = 0;
  _cur = i;
}

void GameManager::next() { select(_cur + 1); }
void GameManager::prev() { select(_cur - 1); }

void GameManager::resetCurrent() { now()->reset(); }

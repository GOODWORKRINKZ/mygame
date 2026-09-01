#include "games.h"

// ============================================================
//  GameManager — список игр и переключение между ними
// ============================================================

GameManager::GameManager() {
  _games[0] = &_tug;
  _games[1] = &_shooter;
  _games[2] = &_pong;
  _games[3] = &_duel;
  _games[4] = &_sprint;
  _games[5] = &_bomb;
  _games[6] = &_sniper;
  _games[7] = &_simon;
  _games[8] = &_runner;
  _games[9] = &_defender;
  _games[10] = &_traffic;
  _games[11] = &_fight;
  _games[12] = &_greed;
  _games[13] = &_chase;
  _games[14] = &_hunt;
  _games[15] = &_tower;
  _games[16] = &_snake;
  _games[17] = &_rhythm;
  _games[18] = &_sync;
  _games[19] = &_siege;

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

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

  for (int i = 0; i < COUNT; i++) {
    _names[i]   = _games[i]->name();
    _players[i] = _games[i]->players();
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

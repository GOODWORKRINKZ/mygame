#pragma once
#include "hardware.h"

// ============================================================
//  Звук игр: мотивы-заставки и банки эффектов.
//
//  Мотив (MusicTrack) играет один раз при старте игры и на экране
//  правил, тихо (MUSIC_DUTY); эффекты его перебивают, после эффекта он
//  продолжается с нужной по времени ноты. Все длительности в мс.
//
//  Банк (SfxBank) делает набор звуков игры уникальным: игра
//  переопределяет часть стандартных эффектов — выстрел, попадание,
//  взрыв, подбор, ошибку, — а остальные берутся из общего набора.
//  Вибрация у эффектов общая и от банка не зависит.
// ============================================================

extern const MusicTrack MUSIC_MENU;
extern const MusicTrack MUSIC_TUG;
extern const MusicTrack MUSIC_SHOOTER;
extern const MusicTrack MUSIC_PONG;
extern const MusicTrack MUSIC_DUEL;
extern const MusicTrack MUSIC_SPRINT;
extern const MusicTrack MUSIC_BOMB;
extern const MusicTrack MUSIC_ORBIT;
extern const MusicTrack MUSIC_SNIPER;
extern const MusicTrack MUSIC_RUNNER;
extern const MusicTrack MUSIC_DEFENDER;
extern const MusicTrack MUSIC_TRAFFIC;
extern const MusicTrack MUSIC_FIGHT;
extern const MusicTrack MUSIC_GREED;
extern const MusicTrack MUSIC_CHASE;
extern const MusicTrack MUSIC_HUNT;
extern const MusicTrack MUSIC_TOWER;
extern const MusicTrack MUSIC_SNAKE;
extern const MusicTrack MUSIC_RHYTHM;
extern const MusicTrack MUSIC_SIEGE;
extern const MusicTrack MUSIC_CATCH;
extern const MusicTrack MUSIC_PONG2D;
extern const MusicTrack MUSIC_COLORS;
extern const MusicTrack MUSIC_DINO;

// ---- банки эффектов ----
extern const SfxBank SND_SHOOTER;
extern const SfxBank SND_PONG;
extern const SfxBank SND_DUEL;      // и СТОП-СИГНАЛ
extern const SfxBank SND_BOMB;
extern const SfxBank SND_ORBIT;
extern const SfxBank SND_SNIPER;
extern const SfxBank SND_RUNNER;
extern const SfxBank SND_DEFENDER;
extern const SfxBank SND_FIGHT;
extern const SfxBank SND_GREED;
extern const SfxBank SND_HUNT;
extern const SfxBank SND_TOWER;
extern const SfxBank SND_SNAKE;
extern const SfxBank SND_RHYTHM;
extern const SfxBank SND_SIEGE;
extern const SfxBank SND_CATCH;
extern const SfxBank SND_PONG2D;
extern const SfxBank SND_COLORS;
extern const SfxBank SND_DINO;

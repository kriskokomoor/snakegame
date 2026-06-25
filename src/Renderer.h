#pragma once

#include "Game.h"
#include "ProfileManager.h"

#include <TFT_eSPI.h>

class Renderer {
public:
  explicit Renderer(TFT_eSPI& tft);

  void begin();
  bool beginSd();
  void drawBirthdaySplash(bool controllerConnected, uint32_t waitTimeMs);
  void drawStartupSplash(bool controllerConnected, uint32_t waitTimeMs);
  void draw(const Game& game, bool controllerReady, const ProfileManager& profiles, uint32_t durationSeconds,
            const char* confirmationMessage);
  void invalidate();

private:
  void drawBanner(const Game& game, bool controllerReady, const ProfileManager& profiles);
  void drawBoardBackground();
  void drawReadyOverlay(const ProfileManager& profiles);
  void drawHighScoresOverlay(const ProfileManager& profiles);
  void drawGameOverOverlay(const Game& game, const ProfileManager& profiles, uint32_t durationSeconds);
  void drawConfirmation(const char* message);
  void drawSnake(const Game& game);
  void drawCell(Cell cell, uint16_t color);
  void drawSnakeHead(Cell cell);
  void clearCell(Cell cell);
  void drawFood(Cell food);
  bool shouldDrawSnakeCell(const Game& game, int index) const;
  bool currentSnakeContains(const Game& game, Cell cell) const;
  bool sameCell(Cell a, Cell b) const;
  const char* getGameStateString(GameState state) const;
  bool drawBmpFromSd(const char* path, int16_t x, int16_t y);
  void drawBirthdaySplashProcedural();

  TFT_eSPI& tft_;
  Cell previousSnake_[MAX_SNAKE_LENGTH];
  int previousSnakeLength_;
  Cell previousFood_;
  int previousScore_;
  char previousPlayer_[PLAYER_NAME_MAX_LEN];
  GameState previousGameState_;
  bool previousControllerReady_;
  bool initialized_;
  bool gameOverModalDrawn_;
  bool readyOverlayDrawn_;
  bool highScoresOverlayDrawn_;
  bool sdAvailable_;
  bool bmpSplashDrawn_;
};

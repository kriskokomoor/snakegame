#pragma once

#include "Game.h"

#include <TFT_eSPI.h>

class Renderer {
public:
  explicit Renderer(TFT_eSPI& tft);

  void begin();
  void drawStartupSplash();
  void draw(const Game& game);

private:
  void drawStaticLayout();
  void drawBoard();
  void drawSnake(const Game& game);
  int previousSnakeIndex(Cell cell) const;
  void drawCell(Cell cell, uint16_t color);
  void drawSnakeHead(Cell cell);
  void clearCell(Cell cell);
  bool currentSnakeContains(const Game& game, Cell cell) const;
  bool shouldDrawSnakeCell(const Game& game, int index) const;
  bool sameCell(Cell a, Cell b) const;
  void drawFood(Cell food);
  void drawScore(int score);
  void drawButtons();
  void drawButton(int x, int y, const char* label, uint16_t color);
  void drawGameOver(int score);

  TFT_eSPI& tft_;
  Cell previousSnake_[MAX_SNAKE_LENGTH];
  int previousSnakeLength_;
  Cell previousFood_;
  int previousScore_;
  bool initialized_;
  bool previousGameOver_;
};

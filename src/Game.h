#pragma once

#include "config.h"

#include <stdint.h>

enum class Direction : uint8_t {
  Up,
  Right,
  Down,
  Left
};

struct Cell {
  int8_t x;
  int8_t y;
};

class Game {
public:
  Game();

  void begin();
  void restart();
  void restart(Direction initialDirection);
  bool update();
  bool setDirection(Direction direction);

  bool isGameOver() const;
  int score() const;
  Direction direction() const;
  const Cell* snake() const;
  int snakeLength() const;
  Cell food() const;

private:
  void placeFood();
  Cell bodyOffset(Direction direction) const;
  bool isSnakeCell(int8_t x, int8_t y) const;
  bool isOpposite(Direction a, Direction b) const;

  Cell snake_[MAX_SNAKE_LENGTH];
  int snakeLength_;
  Cell food_;
  Direction direction_;
  Direction nextDirection_;
  int score_;
  bool gameOver_;
};

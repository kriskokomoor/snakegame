#include "Game.h"

#include <Arduino.h>
#include <esp_system.h>

Game::Game() {
  restart();
}

void Game::begin() {
  randomSeed(esp_random());
  restart();
}

void Game::restart() {
  restart(Direction::Right);
}

void Game::restart(Direction initialDirection) {
  snakeLength_ = 3;
  snake_[0] = {BOARD_COLS / 2, BOARD_ROWS / 2};
  const Cell offset = bodyOffset(initialDirection);
  snake_[1] = {
    static_cast<int8_t>(snake_[0].x + offset.x),
    static_cast<int8_t>(snake_[0].y + offset.y)
  };
  snake_[2] = {
    static_cast<int8_t>(snake_[0].x + offset.x * 2),
    static_cast<int8_t>(snake_[0].y + offset.y * 2)
  };
  direction_ = initialDirection;
  nextDirection_ = direction_;
  score_ = 0;
  gameOver_ = false;
  placeFood();
}

bool Game::update() {
  if (gameOver_) {
    return false;
  }

  direction_ = nextDirection_;

  Cell next = snake_[0];
  switch (direction_) {
    case Direction::Up:
      next.y--;
      break;
    case Direction::Right:
      next.x++;
      break;
    case Direction::Down:
      next.y++;
      break;
    case Direction::Left:
      next.x--;
      break;
  }

  if (next.x < 0 || next.x >= BOARD_COLS || next.y < 0 || next.y >= BOARD_ROWS) {
    gameOver_ = true;
    return true;
  }

  const bool ateFood = next.x == food_.x && next.y == food_.y;
  const int bodyCellsToCheck = ateFood ? snakeLength_ : snakeLength_ - 1;
  for (int i = 0; i < bodyCellsToCheck; ++i) {
    if (snake_[i].x == next.x && snake_[i].y == next.y) {
      gameOver_ = true;
      return true;
    }
  }

  const int newLength = ateFood ? snakeLength_ + 1 : snakeLength_;

  for (int i = newLength - 1; i > 0; --i) {
    snake_[i] = snake_[i - 1];
  }
  snake_[0] = next;
  snakeLength_ = newLength;

  if (ateFood) {
    score_++;
    if (snakeLength_ < MAX_SNAKE_LENGTH) {
      placeFood();
    } else {
      gameOver_ = true;
    }
  }

  return true;
}

bool Game::setDirection(Direction direction) {
  if (gameOver_ || isOpposite(direction_, direction) || isOpposite(nextDirection_, direction)) {
    return false;
  }

  nextDirection_ = direction;
  return true;
}

bool Game::isGameOver() const {
  return gameOver_;
}

int Game::score() const {
  return score_;
}

Direction Game::direction() const {
  return direction_;
}

const Cell* Game::snake() const {
  return snake_;
}

int Game::snakeLength() const {
  return snakeLength_;
}

Cell Game::food() const {
  return food_;
}

void Game::placeFood() {
  if (snakeLength_ >= MAX_SNAKE_LENGTH) {
    food_ = {-1, -1};
    return;
  }

  do {
    food_ = {
      static_cast<int8_t>(random(BOARD_COLS)),
      static_cast<int8_t>(random(BOARD_ROWS))
    };
  } while (isSnakeCell(food_.x, food_.y));
}

Cell Game::bodyOffset(Direction direction) const {
  switch (direction) {
    case Direction::Up:
      return {0, 1};
    case Direction::Right:
      return {-1, 0};
    case Direction::Down:
      return {0, -1};
    case Direction::Left:
      return {1, 0};
  }

  return {-1, 0};
}

bool Game::isSnakeCell(int8_t x, int8_t y) const {
  for (int i = 0; i < snakeLength_; ++i) {
    if (snake_[i].x == x && snake_[i].y == y) {
      return true;
    }
  }
  return false;
}

bool Game::isOpposite(Direction a, Direction b) const {
  return (a == Direction::Up && b == Direction::Down) ||
         (a == Direction::Down && b == Direction::Up) ||
         (a == Direction::Left && b == Direction::Right) ||
         (a == Direction::Right && b == Direction::Left);
}

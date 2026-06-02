#include "Renderer.h"

#include "config.h"

#include <Arduino.h>

Renderer::Renderer(TFT_eSPI& tft)
    : tft_(tft),
      previousSnakeLength_(0),
      previousFood_({-1, -1}),
      previousScore_(-1),
      initialized_(false),
      previousGameOver_(false) {
}

void Renderer::begin() {
  tft_.init();
  tft_.setRotation(SCREEN_ROTATION);
  if (DEBUG_RENDER_REDRAWS) {
    Serial.print("FULL REDRAW begin fillScreen at ms=");
    Serial.println(millis());
  }
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextFont(1);
  tft_.setTextWrap(false);
}

void Renderer::drawStartupSplash() {
  tft_.fillScreen(TFT_BLACK);
  tft_.drawRoundRect(8, 8, SCREEN_WIDTH - 16, SCREEN_HEIGHT - 16, 8, TFT_MAGENTA);
  tft_.drawRoundRect(12, 12, SCREEN_WIDTH - 24, SCREEN_HEIGHT - 24, 6, TFT_YELLOW);

  tft_.fillCircle(48, 48, 7, TFT_RED);
  tft_.fillCircle(272, 192, 7, TFT_RED);

  for (int i = 0; i < 6; ++i) {
    const int x = 84 + i * 16;
    const int y = 164 + (i % 2) * 6;
    tft_.fillRoundRect(x, y, 14, 14, 4, i == 0 ? TFT_GREENYELLOW : TFT_GREEN);
  }

  tft_.setTextFont(1);
  tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft_.setTextSize(3);
  tft_.setCursor(32, 54);
  tft_.print("Happy Birthday");
  tft_.setCursor(74, 86);
  tft_.print("Rachael!");

  tft_.setTextColor(TFT_CYAN, TFT_BLACK);
  tft_.setTextSize(2);
  tft_.setCursor(100, 126);
  tft_.print("Snake Game");

  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.setTextSize(1);
  tft_.setCursor(94, 208);
  tft_.print("Press a direction to start");

  initialized_ = false;
  previousGameOver_ = false;
}

void Renderer::draw(const Game& game) {
  if (DEBUG_LOG_RENDER_DRAWS) {
    Serial.print("Renderer::draw at ms=");
    Serial.println(millis());
  }

  if (!initialized_ || (previousGameOver_ && !game.isGameOver())) {
    drawStaticLayout();
    previousSnakeLength_ = 0;
    previousFood_ = {-1, -1};
    previousScore_ = -1;
    previousGameOver_ = false;
    initialized_ = true;
  }

  if (previousScore_ != game.score()) {
    drawScore(game.score());
    previousScore_ = game.score();
  }

  if (!sameCell(previousFood_, game.food())) {
    clearCell(previousFood_);
    drawFood(game.food());
    previousFood_ = game.food();
  }

  for (int i = 0; i < previousSnakeLength_; ++i) {
    const Cell oldCell = previousSnake_[i];
    if (!currentSnakeContains(game, oldCell) && !sameCell(oldCell, game.food())) {
      clearCell(oldCell);
    }
  }

  drawSnake(game);

  const Cell* snake = game.snake();
  previousSnakeLength_ = game.snakeLength();
  for (int i = 0; i < previousSnakeLength_; ++i) {
    previousSnake_[i] = snake[i];
  }

  if (game.isGameOver()) {
    drawGameOver(game.score());
  }
  previousGameOver_ = game.isGameOver();
}

void Renderer::drawStaticLayout() {
  if (DEBUG_RENDER_REDRAWS) {
    Serial.print("FULL REDRAW static layout at ms=");
    Serial.println(millis());
  }
  tft_.fillScreen(TFT_BLACK);
  drawButtons();
  drawBoard();
}

void Renderer::drawBoard() {
  tft_.fillRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_DARKGREY);
  tft_.drawRect(BOARD_X - 1, BOARD_Y - 1, BOARD_WIDTH + 2, BOARD_HEIGHT + 2, TFT_WHITE);
}

void Renderer::drawSnake(const Game& game) {
  const Cell* snake = game.snake();

  for (int i = game.snakeLength() - 1; i >= 0; --i) {
    if (!shouldDrawSnakeCell(game, i)) {
      continue;
    }

    if (i == 0) {
      drawSnakeHead(snake[i]);
    } else {
      drawCell(snake[i], TFT_GREEN);
    }
  }
}

int Renderer::previousSnakeIndex(Cell cell) const {
  for (int i = 0; i < previousSnakeLength_; ++i) {
    if (sameCell(previousSnake_[i], cell)) {
      return i;
    }
  }
  return -1;
}

void Renderer::drawCell(Cell cell, uint16_t color) {
  if (cell.x < 0 || cell.x >= BOARD_COLS || cell.y < 0 || cell.y >= BOARD_ROWS) {
    return;
  }

  const int x = BOARD_X + cell.x * CELL_SIZE + 1;
  const int y = BOARD_Y + cell.y * CELL_SIZE + 1;
  tft_.fillRect(x, y, CELL_SIZE - 2, CELL_SIZE - 2, color);
}

void Renderer::drawSnakeHead(Cell cell) {
  if (cell.x < 0 || cell.x >= BOARD_COLS || cell.y < 0 || cell.y >= BOARD_ROWS) {
    return;
  }

  const int x = BOARD_X + cell.x * CELL_SIZE + 1;
  const int y = BOARD_Y + cell.y * CELL_SIZE + 1;
  tft_.fillRoundRect(x, y, CELL_SIZE - 2, CELL_SIZE - 2, 3, TFT_GREENYELLOW);
  tft_.fillCircle(x + CELL_SIZE / 2, y + CELL_SIZE / 2, 2, TFT_DARKGREEN);
}

void Renderer::clearCell(Cell cell) {
  drawCell(cell, TFT_DARKGREY);
}

bool Renderer::currentSnakeContains(const Game& game, Cell cell) const {
  const Cell* snake = game.snake();
  for (int i = 0; i < game.snakeLength(); ++i) {
    if (sameCell(snake[i], cell)) {
      return true;
    }
  }
  return false;
}

bool Renderer::shouldDrawSnakeCell(const Game& game, int index) const {
  const Cell* snake = game.snake();
  const int previousIndex = previousSnakeIndex(snake[index]);

  if (previousIndex < 0) {
    return true;
  }

  const bool isHead = index == 0;
  const bool wasHead = previousIndex == 0;
  return isHead != wasHead;
}

bool Renderer::sameCell(Cell a, Cell b) const {
  return a.x == b.x && a.y == b.y;
}

void Renderer::drawFood(Cell food) {
  if (food.x < 0 || food.y < 0) {
    return;
  }

  const int cx = BOARD_X + food.x * CELL_SIZE + CELL_SIZE / 2;
  const int cy = BOARD_Y + food.y * CELL_SIZE + CELL_SIZE / 2;
  tft_.fillCircle(cx, cy, CELL_SIZE / 2 - 2, TFT_RED);
}

void Renderer::drawScore(int score) {
  constexpr int scoreX = RIGHT_BUTTON_X;
  constexpr int scoreY = 8;
  constexpr int scoreWidth = BUTTON_SIZE;
  constexpr int scoreHeight = 36;

  tft_.fillRect(scoreX, scoreY, scoreWidth, scoreHeight, TFT_BLACK);
  tft_.drawRect(scoreX, scoreY, scoreWidth, scoreHeight, TFT_DARKGREY);

  tft_.setTextFont(1);
  tft_.setTextSize(1);
  tft_.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft_.setCursor(scoreX + 9, scoreY + 4);
  tft_.print("SCORE");

  const String scoreText = String(score);
  tft_.setTextSize(2);
  tft_.setTextColor(TFT_YELLOW, TFT_BLACK);
  const int textWidth = scoreText.length() * 12;
  tft_.setCursor(scoreX + (scoreWidth - textWidth) / 2, scoreY + 18);
  tft_.print(scoreText);
}

void Renderer::drawButtons() {
  drawButton(LEFT_BUTTON_X, TOP_BUTTON_Y, "L", TFT_BLUE);
  drawButton(LEFT_BUTTON_X, BOTTOM_BUTTON_Y, "D", TFT_GREEN);
  drawButton(RIGHT_BUTTON_X, TOP_BUTTON_Y, "R", TFT_BLUE);
  drawButton(RIGHT_BUTTON_X, BOTTOM_BUTTON_Y, "U", TFT_RED);
}

void Renderer::drawButton(int x, int y, const char* label, uint16_t color) {
  tft_.fillRoundRect(x, y, BUTTON_SIZE, BUTTON_SIZE, 6, color);
  tft_.drawRoundRect(x, y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_WHITE);

  tft_.setTextFont(1);
  tft_.setTextSize(4);
  tft_.setTextColor(TFT_BLACK, color);
  tft_.setCursor(x + 12, y + 8);
  tft_.print(label);
}

void Renderer::drawGameOver(int score) {
  constexpr int boxWidth = 176;
  constexpr int boxHeight = 74;
  const int x = (SCREEN_WIDTH - boxWidth) / 2;
  const int y = (SCREEN_HEIGHT - boxHeight) / 2;

  tft_.fillRect(x, y, boxWidth, boxHeight, TFT_BLACK);
  tft_.drawRect(x, y, boxWidth, boxHeight, TFT_RED);

  tft_.setTextFont(1);
  tft_.setTextSize(2);
  tft_.setTextColor(TFT_RED, TFT_BLACK);
  tft_.setCursor(SCREEN_WIDTH / 2 - 54, y + 10);
  tft_.print("GAME OVER");

  tft_.setTextSize(1);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  const String scoreText = "Score: " + String(score);
  tft_.setCursor(SCREEN_WIDTH / 2 - (scoreText.length() * 6) / 2, y + 40);
  tft_.print(scoreText);
  tft_.setCursor(SCREEN_WIDTH / 2 - 48, y + 56);
  tft_.print("Touch to restart");
}

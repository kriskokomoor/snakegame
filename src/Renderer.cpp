#include "Renderer.h"
#include "config.h"
#include <Arduino.h>

// Define GFX font structures locally to ensure headers compile 
// and we can render manually if the library is misconfigured.
#ifndef _GFXFONT_H_
#define _GFXFONT_H_
typedef struct {
  uint32_t bitmapOffset;
  uint8_t  width, height;
  uint8_t  xAdvance;
  int8_t   xOffset, yOffset;
} GFXglyph;

typedef struct {
  uint8_t  *bitmap;
  GFXglyph *glyph;
  uint16_t  first, last;
  uint8_t   yAdvance;
} GFXfont;
#endif

// Include standard GFX fonts provided by TFT_eSPI
#include <Fonts/GFXFF/FreeSans9pt7b.h>
#include <Fonts/GFXFF/FreeSansBold12pt7b.h>

namespace {
/**
 * Manual GFX String rendering to bypass TFT_eSPI configuration issues.
 * GFX fonts are transparent by design.
 */
void renderGFXString(TFT_eSPI& tft, const char* str, int16_t x, int16_t y, const GFXfont* font, uint16_t color, uint8_t datum = TL_DATUM) {
  if (!str || !font) return;

  int16_t currX = x;
  
  // Calculate total width for alignment logic
  int16_t totalWidth = 0;
  const char* p = str;
  while (*p) {
    uint8_t c = (uint8_t)*p++;
    if (c >= font->first && c <= font->last) {
      totalWidth += font->glyph[c - font->first].xAdvance;
    }
  }

  // Handle Datums (Horizontal only for this helper)
  if (datum == TC_DATUM || datum == MC_DATUM || datum == BC_DATUM) {
    currX -= (totalWidth / 2);
  } else if (datum == TR_DATUM || datum == MR_DATUM || datum == BR_DATUM) {
    currX -= totalWidth;
  }

  // Handle Vertical Datum alignment (y is baseline for GFX)
  int16_t baselineY = y;
  if (datum == MC_DATUM || datum == ML_DATUM || datum == MR_DATUM) {
      baselineY += (font->yAdvance / 3); // Approximate middle
  }

  // Render glyphs
  while (*str) {
    uint8_t c = (uint8_t)*str++;
    if (c < font->first || c > font->last) {
        if (c == ' ') currX += font->glyph['n' - font->first].xAdvance;
        continue;
    }

    GFXglyph* glyph = &font->glyph[c - font->first];
    uint8_t*  bitmap = font->bitmap;

    uint32_t bo = glyph->bitmapOffset;
    uint8_t  w  = glyph->width, h = glyph->height;
    int8_t   xo = glyph->xOffset, yo = glyph->yOffset;
    uint8_t  xx, yy, bits = 0, bit = 0;

    // Draw glyph pixel by pixel (ensures compatibility)
    for (yy = 0; yy < h; yy++) {
      for (xx = 0; xx < w; xx++) {
        if (!(bit++ & 7)) bits = bitmap[bo++];
        if (bits & 0x80) {
          tft.drawPixel(currX + xo + xx, baselineY + yo + yy, color);
        }
        bits <<= 1;
      }
    }
    currX += glyph->xAdvance;
  }
}

/**
 * Clears a specific line area to prevent transparent font smearing
 */
void clearTextArea(TFT_eSPI& tft, int16_t y, int16_t height, uint16_t bgColor) {
    tft.fillRect(0, y, SCREEN_WIDTH, height, bgColor);
}

} // namespace

Renderer::Renderer(TFT_eSPI& tft)
    : tft_(tft),
      previousSnakeLength_(0),
      previousFood_({-1, -1}),
      previousScore_(-1),
      previousGameState_(GameState::Ready),
      previousControllerReady_(false),
      initialized_(false),
      gameOverModalDrawn_(false) {
}

void Renderer::begin() {
  tft_.init();
  tft_.setRotation(SCREEN_ROTATION);
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextWrap(false);
}

void Renderer::drawStartupSplash(bool controllerConnected, uint32_t waitTimeMs) {
  tft_.fillScreen(TFT_BLACK);
  
  // Using GFX render helper
  renderGFXString(tft_, "Snake Game", SCREEN_WIDTH / 2, 60, &FreeSansBold12pt7b, TFT_YELLOW, TC_DATUM);
  renderGFXString(tft_, "Initializing...", SCREEN_WIDTH / 2, 100, &FreeSans9pt7b, TFT_WHITE, TC_DATUM);

  // Show controller status
  if (controllerConnected) {
    renderGFXString(tft_, "Controller: CONNECTED", SCREEN_WIDTH / 2, 140, &FreeSans9pt7b, TFT_GREEN, TC_DATUM);
  } else {
    uint32_t remainingMs = (waitTimeMs > STARTUP_CONTROLLER_WAIT_MS) ? 0 : (STARTUP_CONTROLLER_WAIT_MS - waitTimeMs);
    uint32_t remainingSec = (remainingMs + 999) / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "Waiting for remote... %us", remainingSec);
    renderGFXString(tft_, buf, SCREEN_WIDTH / 2, 140, &FreeSans9pt7b, TFT_ORANGE, TC_DATUM);
  }
  
  renderGFXString(tft_, "Touch or press direction to play", SCREEN_WIDTH / 2, 180, &FreeSans9pt7b, TFT_CYAN, TC_DATUM);

  initialized_ = false;
}

void Renderer::draw(const Game& game, bool controllerReady) {
  // Full redraw on state change
  if (!initialized_ || previousGameState_ != game.state() || previousControllerReady_ != controllerReady) {
    tft_.fillScreen(TFT_BLACK);
    drawBoardBackground();
    previousSnakeLength_ = 0;
    previousFood_ = {-1, -1};
    previousScore_ = -1;
    gameOverModalDrawn_ = false;
  }

  // Draw banner
  drawBanner(game, controllerReady);

  // Clear old snake cells and food
  if (!sameCell(previousFood_, game.food())) {
    if (previousFood_.x >= 0 && previousFood_.y >= 0) {
      clearCell(previousFood_);
    }
    drawFood(game.food());
    previousFood_ = game.food();
  }

  // Clear old snake body that's no longer there
  for (int i = 0; i < previousSnakeLength_; ++i) {
    const Cell oldCell = previousSnake_[i];
    if (!currentSnakeContains(game, oldCell) && !sameCell(oldCell, game.food())) {
      clearCell(oldCell);
    }
  }

  // Draw new snake
  drawSnake(game);

  // Update previous state
  const Cell* snake = game.snake();
  previousSnakeLength_ = game.snakeLength();
  for (int i = 0; i < previousSnakeLength_; ++i) {
    previousSnake_[i] = snake[i];
  }
  previousScore_ = game.score();
  previousGameState_ = game.state();
  previousControllerReady_ = controllerReady;

  // Re-draw the board border to ensure edge-clearing didn't erase it
  tft_.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_RED);

  initialized_ = true; // Mark as initialized after the first full draw pass

  // At game end, display a game end banner modal (drawn once per GameOver entry)
  if (game.state() == GameState::GameOver && !gameOverModalDrawn_) {
    const int bw = 240;
    const int bh = 90;
    const int bx = (SCREEN_WIDTH - bw) / 2;
    const int by = BOARD_Y + (BOARD_HEIGHT - bh) / 2;

    tft_.fillRect(bx, by, bw, bh, TFT_BLACK);
    tft_.drawRect(bx, by, bw, bh, TFT_RED);
    tft_.drawRect(bx + 2, by + 2, bw - 4, bh - 4, TFT_RED);

    renderGFXString(tft_, "GAME OVER", bx + bw / 2, by + 35, &FreeSansBold12pt7b, TFT_WHITE, TC_DATUM);
    renderGFXString(tft_, "Press any button to Restart", bx + bw / 2, by + 65, &FreeSans9pt7b, TFT_YELLOW, TC_DATUM);
    gameOverModalDrawn_ = true;
  }
}

void Renderer::invalidate() {
  initialized_ = false;
}

void Renderer::drawBanner(const Game& game, bool controllerReady) {
  // Redraw full banner background only if state changed or first pass
  bool stateChanged = !initialized_ || previousGameState_ != game.state() || previousControllerReady_ != controllerReady;
  
  if (stateChanged) {
      tft_.fillRect(0, 0, SCREEN_WIDTH, BANNER_HEIGHT, TFT_NAVY);
      tft_.drawLine(0, BANNER_HEIGHT - 1, SCREEN_WIDTH - 1, BANNER_HEIGHT - 1, TFT_RED);
      
      // 1. BT Status
      renderGFXString(tft_, controllerReady ? "BT:OK" : "BT:--", 5, BANNER_HEIGHT / 2, &FreeSans9pt7b, 
                      controllerReady ? TFT_GREEN : TFT_ORANGE, ML_DATUM);

      // 2. Game state in center
      const char* stateStr = getGameStateString(game.state());
      uint16_t stateColor = TFT_WHITE;
      if (game.state() == GameState::Pause) stateColor = TFT_YELLOW;
      else if (game.state() == GameState::GameOver) stateColor = TFT_RED;
      else if (game.state() == GameState::Play) stateColor = TFT_GREEN;
      
      renderGFXString(tft_, stateStr, SCREEN_WIDTH / 2, BANNER_HEIGHT / 2, &FreeSans9pt7b, stateColor, MC_DATUM);
  }

  // 3. Score right justified - Only redraw if it changed OR the banner was just cleared
  if (stateChanged || previousScore_ != game.score()) {
      // GFX fonts are transparent, so we must clear the score area in the navy banner
      tft_.fillRect(SCREEN_WIDTH - 100, 0, 100, BANNER_HEIGHT - 1, TFT_NAVY);
      
      char scoreBuf[16];
      snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", game.score());
      renderGFXString(tft_, scoreBuf, SCREEN_WIDTH - 5, BANNER_HEIGHT / 2, &FreeSans9pt7b, TFT_CYAN, MR_DATUM);
  }
}

void Renderer::drawBoardBackground() {
  tft_.fillRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_DARKGREY);
  tft_.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_RED);
}

void Renderer::drawBirthdaySplash(bool controllerConnected, uint32_t waitTimeMs) {
  static uint32_t lastFullRedraw = 0;
  static uint32_t lastSec = 0xFFFFFFFF;
  static bool lastConnected = false;

  bool needsFullRedraw = (waitTimeMs - lastFullRedraw > 1500) || !initialized_;
  uint32_t remainingSec = (BIRTHDAY_SPLASH_TIMEOUT_MS - waitTimeMs + 999) / 1000;
  
  if (needsFullRedraw) {
    if (!initialized_) {
      tft_.fillScreen(TFT_MAGENTA);  // only clear on first draw
    }
    for(int i=0; i<40; i++) {
        tft_.fillCircle(random(SCREEN_WIDTH), random(SCREEN_HEIGHT), random(2,5), random(0xFFFF));
    }
    lastFullRedraw = waitTimeMs;
    
    renderGFXString(tft_, "Happy Birthday", SCREEN_WIDTH / 2, 60, &FreeSansBold12pt7b, TFT_WHITE, TC_DATUM);
    renderGFXString(tft_, "Rachael!", SCREEN_WIDTH / 2, 95, &FreeSansBold12pt7b, TFT_YELLOW, TC_DATUM);
    renderGFXString(tft_, "Press any button to start", SCREEN_WIDTH / 2, 190, &FreeSans9pt7b, TFT_WHITE, TC_DATUM);
  }

  // Only update status line if something changed to prevent flashing
  if (needsFullRedraw || controllerConnected != lastConnected || remainingSec != lastSec) {
    tft_.fillRect(0, 130, SCREEN_WIDTH, 30, TFT_MAGENTA);
    if (controllerConnected) {
      renderGFXString(tft_, "Controller: CONNECTED", SCREEN_WIDTH / 2, 150, &FreeSans9pt7b, TFT_GREEN, TC_DATUM);
    } else {
      char buf[32];
      snprintf(buf, sizeof(buf), "Waiting for remote... %us", remainingSec);
      renderGFXString(tft_, buf, SCREEN_WIDTH / 2, 150, &FreeSans9pt7b, TFT_CYAN, MC_DATUM);
    }
    lastConnected = controllerConnected;
    lastSec = remainingSec;
  }

  initialized_ = false; // Ensure full redraw after splash
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

void Renderer::drawCell(Cell cell, uint16_t color) {
  if (cell.x < 0 || cell.x >= BOARD_COLS || cell.y < 0 || cell.y >= BOARD_ROWS) {
    return;
  }

  const int x = BOARD_X + cell.x * CELL_SIZE;
  const int y = BOARD_Y + cell.y * CELL_SIZE;
  constexpr int SEGMENT_GAP = 1;

  tft_.fillRect(
    x + SEGMENT_GAP,
    y + SEGMENT_GAP,
    CELL_SIZE - (SEGMENT_GAP * 2),
    CELL_SIZE - (SEGMENT_GAP * 2),
    color
  );
  // tft_.fillRect(x, y, CELL_SIZE, CELL_SIZE, color);
}

void Renderer::drawSnakeHead(Cell cell) {
  if (cell.x < 0 || cell.x >= BOARD_COLS || cell.y < 0 || cell.y >= BOARD_ROWS) {
    return;
  }

  const int x = BOARD_X + cell.x * CELL_SIZE;
  const int y = BOARD_Y + cell.y * CELL_SIZE;
  tft_.fillRect(x, y, CELL_SIZE, CELL_SIZE, TFT_GREENYELLOW);
  // Draw a dot for the eye
  tft_.fillCircle(x + CELL_SIZE - 2, y + CELL_SIZE - 2, 1, TFT_BLACK);
}

void Renderer::clearCell(Cell cell) {
  drawCell(cell, TFT_DARKGREY);
}

void Renderer::drawFood(Cell food) {
  if (food.x < 0 || food.x >= BOARD_COLS || food.y < 0 || food.y >= BOARD_ROWS) {
    return;
  }

  const int x = BOARD_X + food.x * CELL_SIZE;
  const int y = BOARD_Y + food.y * CELL_SIZE;
  tft_.fillRect(x, y, CELL_SIZE, CELL_SIZE, TFT_RED);
}

bool Renderer::shouldDrawSnakeCell(const Game& game, int index) const {
  const Cell* snake = game.snake();
  const Cell cell = snake[index];
  
  // Check if this cell was in the previous snake at a different index
  for (int i = 0; i < previousSnakeLength_; ++i) {
    if (sameCell(previousSnake_[i], cell)) {
      // Only redraw if index changed (head became body or vice versa)
      return i != index;
    }
  }
  
  // New cell, always draw
  return true;
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

bool Renderer::sameCell(Cell a, Cell b) const {
  return a.x == b.x && a.y == b.y;
}

const char* Renderer::getGameStateString(GameState state) const {
  switch (state) {
    case GameState::Ready:
      return "READY";
    case GameState::Play:
      return "PLAY";
    case GameState::Pause:
      return "PAUSE";
    case GameState::GameOver:
      return "OVER";
    default:
      return "????";
  }
}

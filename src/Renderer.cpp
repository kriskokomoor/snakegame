#include "Renderer.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>

static SPIClass sdSpi(VSPI);

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
      gameOverModalDrawn_(false),
      readyOverlayDrawn_(false),
      highScoresOverlayDrawn_(false),
      sdAvailable_(false),
      bmpSplashDrawn_(false) {
  previousPlayer_[0] = '\0';
}

void Renderer::begin() {
  tft_.init();
  tft_.setRotation(SCREEN_ROTATION);
  tft_.fillScreen(TFT_BLACK);
  tft_.setTextWrap(false);
}

bool Renderer::beginSd() {
  sdSpi.begin(SD_SPI_CLK, SD_SPI_MISO, SD_SPI_MOSI, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSpi, 20000000)) {
    Serial.println("SD: init failed – no card or wiring issue");
    sdAvailable_ = false;
    return false;
  }
  sdAvailable_ = true;
  Serial.println("SD: ready");
  return true;
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

void Renderer::draw(const Game& game, bool controllerReady, const ProfileManager& profiles, uint32_t durationSeconds,
                    const char* confirmationMessage) {
  // Full redraw on state change
  const bool playerChanged = strcmp(previousPlayer_, profiles.currentPlayerName()) != 0;
  if (!initialized_ || previousGameState_ != game.state() || previousControllerReady_ != controllerReady ||
      playerChanged) {
    tft_.fillScreen(TFT_BLACK);
    drawBoardBackground();
    previousSnakeLength_ = 0;
    previousFood_ = {-1, -1};
    previousScore_ = -1;
    gameOverModalDrawn_ = false;
    readyOverlayDrawn_ = false;
    highScoresOverlayDrawn_ = false;
  }

  // Draw banner
  drawBanner(game, controllerReady, profiles);

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
  snprintf(previousPlayer_, sizeof(previousPlayer_), "%s", profiles.currentPlayerName());
  previousGameState_ = game.state();
  previousControllerReady_ = controllerReady;

  // Re-draw the board border to ensure edge-clearing didn't erase it
  tft_.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_RED);

  initialized_ = true; // Mark as initialized after the first full draw pass

  if (game.state() == GameState::Ready && !readyOverlayDrawn_) {
    drawReadyOverlay(profiles);
    readyOverlayDrawn_ = true;
  }

  if (game.state() == GameState::HighScores && !highScoresOverlayDrawn_) {
    drawHighScoresOverlay(profiles);
    highScoresOverlayDrawn_ = true;
  }

  // At game end, display a game end banner modal (drawn once per GameOver entry)
  if (game.state() == GameState::GameOver && !gameOverModalDrawn_) {
    drawGameOverOverlay(game, profiles, durationSeconds);
    gameOverModalDrawn_ = true;
  }

  if (confirmationMessage != nullptr && confirmationMessage[0] != '\0') {
    drawConfirmation(confirmationMessage);
  }
}

void Renderer::invalidate() {
  initialized_ = false;
}

void Renderer::drawBanner(const Game& game, bool controllerReady, const ProfileManager& profiles) {
  // Redraw full banner background only if state changed or first pass
  bool stateChanged = !initialized_ || previousGameState_ != game.state() ||
                      previousControllerReady_ != controllerReady ||
                      strcmp(previousPlayer_, profiles.currentPlayerName()) != 0;
  
  if (stateChanged) {
      tft_.fillRect(0, 0, SCREEN_WIDTH, BANNER_HEIGHT, TFT_NAVY);
      tft_.drawLine(0, BANNER_HEIGHT - 1, SCREEN_WIDTH - 1, BANNER_HEIGHT - 1, TFT_RED);
      
      char playerBuf[12];
      snprintf(playerBuf, sizeof(playerBuf), "P:%s", profiles.compactPlayerName());
      renderGFXString(tft_, playerBuf, 5, BANNER_HEIGHT / 2, &FreeSans9pt7b,
                      controllerReady ? TFT_GREEN : TFT_ORANGE, ML_DATUM);

      // 1. Game state in center
      const char* stateStr = getGameStateString(game.state());
      uint16_t stateColor = TFT_WHITE;
      if (game.state() == GameState::Pause) stateColor = TFT_YELLOW;
      else if (game.state() == GameState::GameOver) stateColor = TFT_RED;
      else if (game.state() == GameState::Play) stateColor = TFT_GREEN;
      
      renderGFXString(tft_, stateStr, SCREEN_WIDTH / 2, BANNER_HEIGHT / 2, &FreeSans9pt7b, stateColor, MC_DATUM);
  }

  // 2. Score right justified - Only redraw if it changed OR the banner was just cleared
  if (stateChanged || previousScore_ != game.score()) {
      // GFX fonts are transparent, so we must clear the score area in the navy banner
      tft_.fillRect(SCREEN_WIDTH - 100, 0, 100, BANNER_HEIGHT - 1, TFT_NAVY);
      if (game.state() == GameState::HighScores) return;
      
      char scoreBuf[16];
      snprintf(scoreBuf, sizeof(scoreBuf), "Score: %d", game.score());
      renderGFXString(tft_, scoreBuf, SCREEN_WIDTH - 5, BANNER_HEIGHT / 2, &FreeSans9pt7b, TFT_CYAN, MR_DATUM);
  }
}

void Renderer::drawReadyOverlay(const ProfileManager& profiles) {
  const int bx = 18;
  const int by = BOARD_Y + 18;
  const int bw = SCREEN_WIDTH - 36;
  const int bh = 170;

  tft_.fillRect(bx, by, bw, bh, TFT_BLACK);
  tft_.drawRect(bx, by, bw, bh, TFT_DARKCYAN);
  renderGFXString(tft_, "SNAKE", bx + bw / 2, by + 28, &FreeSansBold12pt7b, TFT_GREENYELLOW, TC_DATUM);

  char playerBuf[32];
  snprintf(playerBuf, sizeof(playerBuf), "PLAYER: %s", profiles.currentPlayerName());
  renderGFXString(tft_, playerBuf, bx + bw / 2, by + 55, &FreeSans9pt7b, TFT_CYAN, TC_DATUM);

  renderGFXString(tft_, "HIGH SCORES", bx + bw / 2, by + 82, &FreeSans9pt7b, TFT_YELLOW, TC_DATUM);

  if (profiles.scoreCount() == 0) {
    renderGFXString(tft_, "No scores yet", bx + bw / 2, by + 112, &FreeSans9pt7b, TFT_WHITE, TC_DATUM);
  } else {
    const uint8_t rows = profiles.scoreCount() < 4 ? profiles.scoreCount() : 4;
    for (uint8_t i = 0; i < rows; ++i) {
      const HighScoreEntry& score = profiles.scoreAt(i);
      char timeBuf[8];
      char row[64];
      ProfileManager::formatDuration(score.durationSeconds, timeBuf, sizeof(timeBuf));
      snprintf(row, sizeof(row), "%u %s  %d  %s  L:%d",
               static_cast<unsigned>(i + 1), score.player, score.score, timeBuf, score.snakeLength);
      renderGFXString(tft_, row, bx + 14, by + 107 + i * 18, &FreeSans9pt7b, TFT_WHITE, ML_DATUM);
    }
  }
}

void Renderer::drawHighScoresOverlay(const ProfileManager& profiles) {
  tft_.fillRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_BLACK);
  tft_.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_DARKCYAN);

  renderGFXString(tft_, "TOP 10 SCORES", SCREEN_WIDTH / 2, BOARD_Y + 22, &FreeSansBold12pt7b, TFT_YELLOW, TC_DATUM);
  renderGFXString(tft_, "# PLAYER SCORE TIME LEN", 12, BOARD_Y + 48, &FreeSans9pt7b, TFT_CYAN, ML_DATUM);
  tft_.drawLine(8, BOARD_Y + 58, SCREEN_WIDTH - 8, BOARD_Y + 58, TFT_DARKCYAN);

  if (profiles.scoreCount() == 0) {
    renderGFXString(tft_, "No scores yet", SCREEN_WIDTH / 2, BOARD_Y + 115, &FreeSans9pt7b, TFT_WHITE, TC_DATUM);
    renderGFXString(tft_, "Press SELECT or START", SCREEN_WIDTH / 2, BOARD_Y + 145, &FreeSans9pt7b, TFT_GREEN, TC_DATUM);
    return;
  }

  const uint8_t rows = profiles.scoreCount() < MAX_HIGH_SCORES ? profiles.scoreCount() : MAX_HIGH_SCORES;
  for (uint8_t i = 0; i < rows; ++i) {
    const HighScoreEntry& score = profiles.scoreAt(i);
    char timeBuf[8];
    char row[64];
    ProfileManager::formatDuration(score.durationSeconds, timeBuf, sizeof(timeBuf));
    snprintf(row, sizeof(row), "%2u %-7s %4d %5s %3d",
             static_cast<unsigned>(i + 1), score.player, score.score, timeBuf, score.snakeLength);
    renderGFXString(tft_, row, 12, BOARD_Y + 72 + i * 14, &FreeSans9pt7b, TFT_WHITE, ML_DATUM);
  }

  renderGFXString(tft_, "SELECT/START: BACK", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 8, &FreeSans9pt7b, TFT_GREEN, BC_DATUM);
}

void Renderer::drawGameOverOverlay(const Game& game, const ProfileManager& profiles, uint32_t durationSeconds) {
  const int bw = 252;
  const int bh = 150;
  const int bx = (SCREEN_WIDTH - bw) / 2;
  const int by = BOARD_Y + (BOARD_HEIGHT - bh) / 2;
  char timeBuf[8];
  char row[48];

  ProfileManager::formatDuration(durationSeconds, timeBuf, sizeof(timeBuf));

  tft_.fillRect(bx, by, bw, bh, TFT_BLACK);
  tft_.drawRect(bx, by, bw, bh, TFT_RED);
  tft_.drawRect(bx + 2, by + 2, bw - 4, bh - 4, TFT_RED);

  renderGFXString(tft_, "GAME OVER", bx + bw / 2, by + 27, &FreeSansBold12pt7b, TFT_WHITE, TC_DATUM);
  snprintf(row, sizeof(row), "PLAYER: %s", profiles.currentPlayerName());
  renderGFXString(tft_, row, bx + bw / 2, by + 52, &FreeSans9pt7b, TFT_CYAN, TC_DATUM);
  snprintf(row, sizeof(row), "SCORE: %d", game.score());
  renderGFXString(tft_, row, bx + 22, by + 78, &FreeSans9pt7b, TFT_YELLOW, ML_DATUM);
  snprintf(row, sizeof(row), "TIME: %s", timeBuf);
  renderGFXString(tft_, row, bx + 132, by + 78, &FreeSans9pt7b, TFT_YELLOW, ML_DATUM);
  snprintf(row, sizeof(row), "LENGTH: %d", game.snakeLength());
  renderGFXString(tft_, row, bx + bw / 2, by + 105, &FreeSans9pt7b, TFT_WHITE, TC_DATUM);
  renderGFXString(tft_, "Press action to restart", bx + bw / 2, by + 132, &FreeSans9pt7b, TFT_GREEN, TC_DATUM);
}

void Renderer::drawConfirmation(const char* message) {
  const int bw = 270;
  const int bh = 54;
  const int bx = (SCREEN_WIDTH - bw) / 2;
  const int by = BOARD_Y + 8;

  tft_.fillRect(bx, by, bw, bh, TFT_BLACK);
  tft_.drawRect(bx, by, bw, bh, TFT_YELLOW);
  renderGFXString(tft_, message, bx + bw / 2, by + 35, &FreeSans9pt7b, TFT_YELLOW, TC_DATUM);
}

void Renderer::drawBoardBackground() {
  tft_.fillRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_DARKGREY);
  tft_.drawRect(BOARD_X, BOARD_Y, BOARD_WIDTH, BOARD_HEIGHT, TFT_RED);
}

void Renderer::drawBirthdaySplash(bool controllerConnected, uint32_t waitTimeMs) {
  static bool lastConnected = false;
  static uint32_t lastSec = 0xFFFFFFFF;

  if (!bmpSplashDrawn_) {
    bool drawn = sdAvailable_ &&
                 drawBmpFromSd("/images/Happy_Birthday_Rachael_VFD_320x240.bmp", 0, 0);
    if (!drawn) {
      drawBirthdaySplashProcedural();
    }
    bmpSplashDrawn_ = true;
    // Unmount before input.begin() reconfigures VSPI for the touch controller
    SD.end();
    sdAvailable_ = false;
    // Force status overlay to paint on the very next check
    lastConnected = !controllerConnected;
    lastSec = 0xFFFFFFFF;
  }

  uint32_t remainingSec = (BIRTHDAY_SPLASH_TIMEOUT_MS > waitTimeMs)
                          ? (BIRTHDAY_SPLASH_TIMEOUT_MS - waitTimeMs + 999) / 1000
                          : 0;

  if (controllerConnected != lastConnected || remainingSec != lastSec) {
    constexpr int16_t overlayY = SCREEN_HEIGHT - 30;
    tft_.fillRect(0, overlayY, SCREEN_WIDTH, 30, TFT_BLACK);
    if (controllerConnected) {
      renderGFXString(tft_, "Controller: CONNECTED",
                      SCREEN_WIDTH / 2, overlayY + 20,
                      &FreeSans9pt7b, TFT_GREEN, TC_DATUM);
    } else {
      char buf[48];
      snprintf(buf, sizeof(buf), "Waiting for remote... %us", (unsigned)remainingSec);
      renderGFXString(tft_, buf,
                      SCREEN_WIDTH / 2, overlayY + 20,
                      &FreeSans9pt7b, TFT_CYAN, TC_DATUM);
    }
    lastConnected = controllerConnected;
    lastSec = remainingSec;
  }

  initialized_ = false;
}

void Renderer::drawBirthdaySplashProcedural() {
  tft_.fillScreen(TFT_MAGENTA);
  renderGFXString(tft_, "Happy Birthday",          SCREEN_WIDTH / 2, 60,  &FreeSansBold12pt7b, TFT_WHITE,  TC_DATUM);
  renderGFXString(tft_, "Rachael!",                SCREEN_WIDTH / 2, 95,  &FreeSansBold12pt7b, TFT_YELLOW, TC_DATUM);
  renderGFXString(tft_, "Press any button to start", SCREEN_WIDTH / 2, 190, &FreeSans9pt7b,     TFT_WHITE,  TC_DATUM);
}

bool Renderer::drawBmpFromSd(const char* path, int16_t x, int16_t y) {
  if (!sdAvailable_) return false;

  File f = SD.open(path, FILE_READ);
  if (!f) {
    Serial.printf("BMP: cannot open '%s'\n", path);
    return false;
  }

  uint8_t hdr[54];
  if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
    Serial.println("BMP: bad or truncated header");
    f.close();
    return false;
  }

  // Explicit little-endian reads — safe on any alignment
  auto r16 = [](const uint8_t* p) -> uint16_t {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
  };
  auto r32 = [](const uint8_t* p) -> uint32_t {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
  };

  uint32_t dataOffset = r32(hdr + 10);
  int32_t  imgWidth   = (int32_t)r32(hdr + 18);
  int32_t  imgHeight  = (int32_t)r32(hdr + 22);
  uint16_t bpp        = r16(hdr + 28);
  uint32_t compression = r32(hdr + 30);

  if (bpp != 24 || compression != 0) {
    Serial.printf("BMP: unsupported (bpp=%d compression=%d)\n", bpp, compression);
    f.close();
    return false;
  }

  bool topDown = (imgHeight < 0);
  if (topDown) imgHeight = -imgHeight;

  if (imgWidth > SCREEN_WIDTH || imgHeight > SCREEN_HEIGHT) {
    Serial.printf("BMP: image %dx%d exceeds screen %dx%d\n",
                  imgWidth, imgHeight, SCREEN_WIDTH, SCREEN_HEIGHT);
    f.close();
    return false;
  }

  // Row stride is padded to a 4-byte boundary
  uint32_t rowStride = ((uint32_t)(imgWidth * 3) + 3u) & ~3u;

  // Static buffers avoid stack pressure on embedded target
  static uint8_t  rowBuf[SCREEN_WIDTH * 3];
  static uint16_t pixBuf[SCREEN_WIDTH];

  const int32_t drawW = min((int32_t)SCREEN_WIDTH  - x, imgWidth);
  const int32_t drawH = min((int32_t)SCREEN_HEIGHT - y, imgHeight);

  tft_.startWrite();
  tft_.setAddrWindow(x, y, drawW, drawH);

  for (int32_t row = 0; row < drawH; ++row) {
    int32_t bmpRow = topDown ? row : (imgHeight - 1 - row);
    f.seek(dataOffset + (uint32_t)bmpRow * rowStride);
    f.read(rowBuf, (uint32_t)(imgWidth * 3));

    for (int32_t col = 0; col < drawW; ++col) {
      uint8_t b = rowBuf[col * 3 + 0];
      uint8_t g = rowBuf[col * 3 + 1];
      uint8_t r = rowBuf[col * 3 + 2];
      pixBuf[col] = tft_.color565(r, g, b);
    }
    tft_.pushColors(pixBuf, drawW, true);
  }

  tft_.endWrite();
  f.close();
  Serial.printf("BMP: rendered '%s' (%dx%d)\n", path, imgWidth, imgHeight);
  return true;
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
  if (cell.x < 0 || cell.x >= BOARD_COLS || cell.y < 0 || cell.y >= BOARD_ROWS) {
    return;
  }
  const int x = BOARD_X + cell.x * CELL_SIZE;
  const int y = BOARD_Y + cell.y * CELL_SIZE;
  tft_.fillRect(x, y, CELL_SIZE, CELL_SIZE, TFT_DARKGREY);
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
    case GameState::HighScores:
      return "SCORES";
    default:
      return "????";
  }
}

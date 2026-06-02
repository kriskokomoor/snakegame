#pragma once

#include <stdint.h>

constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;
constexpr int SCREEN_ROTATION = 1;

constexpr int TFT_BACKLIGHT_PIN = 21;
constexpr int TFT_SPI_MISO = 12;
constexpr int TFT_SPI_MOSI = 13;
constexpr int TFT_SPI_SCLK = 14;
constexpr int TFT_DISPLAY_CS = 15;

constexpr int TOUCH_SPI_MISO = 39;
constexpr int TOUCH_SPI_MOSI = 32;
constexpr int TOUCH_SPI_SCLK = 25;
constexpr int TOUCH_CS_PIN = 33;
constexpr int TOUCH_IRQ_PIN = 36;

constexpr int BOARD_COLS = 16;
constexpr int BOARD_ROWS = 16;
constexpr int CELL_SIZE = 12;
constexpr int BOARD_WIDTH = BOARD_COLS * CELL_SIZE;
constexpr int BOARD_HEIGHT = BOARD_ROWS * CELL_SIZE;
constexpr int BOARD_X = (SCREEN_WIDTH - BOARD_WIDTH) / 2;
constexpr int BOARD_Y = 28;

constexpr int BUTTON_SIZE = 48;
constexpr int LEFT_BUTTON_X = 8;
constexpr int RIGHT_BUTTON_X = SCREEN_WIDTH - LEFT_BUTTON_X - BUTTON_SIZE;
constexpr int TOP_BUTTON_Y = 58;
constexpr int BOTTOM_BUTTON_Y = 136;

constexpr uint32_t INITIAL_GAME_TICK_MS = 500;
constexpr uint32_t MIN_GAME_TICK_MS = 120;
constexpr uint32_t TICK_DECREASE_PER_FOOD_MS = 20;
constexpr uint32_t STARTUP_SPLASH_MS = 5500;
constexpr int MAX_SNAKE_LENGTH = BOARD_COLS * BOARD_ROWS;

constexpr bool DEBUG_RENDER_REDRAWS = true;
constexpr bool DEBUG_LOG_RENDER_DRAWS = true;
constexpr bool DEBUG_DISABLE_TOUCH_POLLING = false;
constexpr bool DEBUG_DISABLE_GAME_RENDER = false;
constexpr bool DEBUG_REASSERT_BACKLIGHT = false;
constexpr uint32_t DEBUG_BACKLIGHT_REASSERT_MS = 250;

constexpr uint16_t TOUCH_THRESHOLD = 80;
constexpr uint32_t TOUCH_SPI_FREQUENCY = 2500000;

constexpr int TOUCH_RAW_X_MIN = 200;
constexpr int TOUCH_RAW_X_MAX = 3800;
constexpr int TOUCH_RAW_Y_MIN = 200;
constexpr int TOUCH_RAW_Y_MAX = 3800;
constexpr bool TOUCH_SWAP_XY = true;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = false;

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

// Layout: 28px banner + game board
constexpr int BANNER_HEIGHT = 28;
constexpr int BOARD_COLS = 32;
constexpr int BOARD_ROWS = 21;
constexpr int CELL_SIZE = 10;
constexpr int BOARD_WIDTH = BOARD_COLS * CELL_SIZE;   // 320px
constexpr int BOARD_HEIGHT = BOARD_ROWS * CELL_SIZE;  // 210px
constexpr int BOARD_X = 0;
constexpr int BOARD_Y = BANNER_HEIGHT;

// Invisible touch zones for touchscreen fallback
// Top-left corner for start/resume
constexpr int TOUCH_START_ZONE_X = 0;
constexpr int TOUCH_START_ZONE_Y = BANNER_HEIGHT;
constexpr int TOUCH_START_ZONE_WIDTH = 40;
constexpr int TOUCH_START_ZONE_HEIGHT = 40;

// Direction zones (invisible buttons at edges)
// Left side
constexpr int TOUCH_LEFT_ZONE_X = 0;
constexpr int TOUCH_LEFT_ZONE_Y = BANNER_HEIGHT + 80;
constexpr int TOUCH_LEFT_ZONE_WIDTH = 40;
constexpr int TOUCH_LEFT_ZONE_HEIGHT = 60;

// Right side
constexpr int TOUCH_RIGHT_ZONE_X = SCREEN_WIDTH - 40;
constexpr int TOUCH_RIGHT_ZONE_Y = BANNER_HEIGHT + 80;
constexpr int TOUCH_RIGHT_ZONE_WIDTH = 40;
constexpr int TOUCH_RIGHT_ZONE_HEIGHT = 60;

// Top area for UP
constexpr int TOUCH_UP_ZONE_X = SCREEN_WIDTH / 2 - 30;
constexpr int TOUCH_UP_ZONE_Y = BANNER_HEIGHT;
constexpr int TOUCH_UP_ZONE_WIDTH = 60;
constexpr int TOUCH_UP_ZONE_HEIGHT = 40;

// Bottom area for DOWN
constexpr int TOUCH_DOWN_ZONE_X = SCREEN_WIDTH / 2 - 30;
constexpr int TOUCH_DOWN_ZONE_Y = SCREEN_HEIGHT - 40;
constexpr int TOUCH_DOWN_ZONE_WIDTH = 60;
constexpr int TOUCH_DOWN_ZONE_HEIGHT = 40;

constexpr uint32_t INITIAL_GAME_TICK_MS = 500;
constexpr uint32_t MIN_GAME_TICK_MS = 120;
constexpr uint32_t TICK_DECREASE_PER_FOOD_MS = 20;
constexpr uint32_t STARTUP_SPLASH_MS = 3000;
constexpr uint32_t BIRTHDAY_SPLASH_TIMEOUT_MS = 60000;
constexpr uint32_t STARTUP_CONTROLLER_WAIT_MS = 60000;
constexpr uint32_t PLAYER_HOTKEY_WINDOW_MS = 2000;
constexpr uint32_t PLAYER_CONFIRMATION_MS = 1500;
constexpr uint8_t PLAYER_HOTKEY_PRESS_COUNT = 3;
constexpr uint32_t GAME_OVER_INPUT_LOCKOUT_MS = 2000;
constexpr int MAX_SNAKE_LENGTH = BOARD_COLS * BOARD_ROWS;

constexpr bool DEBUG_RENDER_REDRAWS = false;
constexpr bool DEBUG_LOG_RENDER_DRAWS = false;
constexpr bool DEBUG_DISABLE_TOUCH_POLLING = false;
constexpr bool DEBUG_DISABLE_GAME_RENDER = false;
constexpr bool DEBUG_REASSERT_BACKLIGHT = false;
constexpr bool DEBUG_FORGET_BLUETOOTH_KEYS_ON_BOOT = true;
constexpr bool DEBUG_ENABLE_BLUETOOTH_ALLOWLIST = false;
constexpr bool DEBUG_ENABLE_BLUETOOTH_INQUIRY_AUTOCONNECT = false;
constexpr uint8_t DEBUG_BLUETOOTH_ALLOWLIST_ADDR[6] = {0xE4, 0x17, 0xD8, 0x7A, 0x1C, 0xFA};
constexpr uint32_t DEBUG_BACKLIGHT_REASSERT_MS = 250;

constexpr uint16_t TOUCH_THRESHOLD = 80;
constexpr uint8_t TOUCH_PRESS_DEBOUNCE_SAMPLES = 3;
constexpr uint8_t TOUCH_RELEASE_DEBOUNCE_SAMPLES = 2;
constexpr uint32_t TOUCH_SPI_FREQUENCY = 2500000;

// SD card — VSPI default pins on CYD (ESP32-2432S028)
constexpr int SD_SPI_CLK  = 18;
constexpr int SD_SPI_MISO = 19;
constexpr int SD_SPI_MOSI = 23;
constexpr int SD_CS_PIN   = 5;

constexpr int TOUCH_RAW_X_MIN = 200;
constexpr int TOUCH_RAW_X_MAX = 3800;
constexpr int TOUCH_RAW_Y_MIN = 200;
constexpr int TOUCH_RAW_Y_MAX = 3800;
constexpr bool TOUCH_SWAP_XY = true;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = false;

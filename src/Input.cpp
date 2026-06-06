#include "Input.h"

#include "config.h"

#include <Arduino.h>

namespace {
uint16_t mapRawAxis(uint16_t raw, int rawMin, int rawMax, int screenMax, bool invert) {
  long value = map(raw, rawMin, rawMax, 0, screenMax - 1);
  value = constrain(value, 0, screenMax - 1);

  if (invert) {
    value = screenMax - 1 - value;
  }

  return static_cast<uint16_t>(value);
}
}

GameInput::GameInput(TFT_eSPI& tft)
    : tft_(tft),
      touchSpi_(VSPI),
      touchCs_(-1),
      touched_(false),
      directionAvailable_(false),
      actionAvailable_(false),
      pauseRequested_(false),
      direction_(Direction::Right),
      action_(ControllerCommand::NONE) {
}

void GameInput::begin() {
  beginTouchController();
  ControllerManager::begin();
}

void GameInput::update() {
  // Check controller first
  ControllerCommand command = ControllerManager::consumeCommand();
  directionAvailable_ = false;
  actionAvailable_ = false;
  pauseRequested_ = false;
  action_ = ControllerCommand::NONE;

  if (command != ControllerCommand::NONE) {
    switch (command) {
      case ControllerCommand::UP:
        direction_ = Direction::Up;
        directionAvailable_ = true;
        break;
      case ControllerCommand::DOWN:
        direction_ = Direction::Down;
        directionAvailable_ = true;
        break;
      case ControllerCommand::LEFT:
        direction_ = Direction::Left;
        directionAvailable_ = true;
        break;
      case ControllerCommand::RIGHT:
        direction_ = Direction::Right;
        directionAvailable_ = true;
        break;
      case ControllerCommand::ACTION_1:
        pauseRequested_ = true; // ACTION_1 -> pause/resume
        break;
      case ControllerCommand::ACTION_2:
        actionAvailable_ = true; // ACTION_2 -> start/restart
        action_ = ControllerCommand::ACTION_2;
        break;
      case ControllerCommand::START:
      case ControllerCommand::SELECT:
        pauseRequested_ = true; // Reserved for menus, but use as pause fallback
        break;
      case ControllerCommand::NONE:
      default:
        break;
    }
  }

  // Fall back to touchscreen input
  if (directionAvailable_ || actionAvailable_ || pauseRequested_) {
    return;  // Already have input from controller
  }

  if (DEBUG_DISABLE_TOUCH_POLLING) {
    touched_ = false;
    return;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  const uint16_t pressure = readPressure();

  touched_ = pressure > TOUCH_THRESHOLD;
  directionAvailable_ = false;
  actionAvailable_ = false;
  pauseRequested_ = false;

  if (!touched_) {
    return;
  }

  readRawPoint(rawX, rawY);

  if (TOUCH_SWAP_XY) {
    x = mapRawAxis(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, SCREEN_WIDTH, TOUCH_INVERT_X);
    y = mapRawAxis(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, SCREEN_HEIGHT, TOUCH_INVERT_Y);
  } else {
    x = mapRawAxis(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, SCREEN_WIDTH, TOUCH_INVERT_X);
    y = mapRawAxis(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, SCREEN_HEIGHT, TOUCH_INVERT_Y);
  }

  // Check invisible touch zones for direction input
  if (pointInZone(x, y, TOUCH_UP_ZONE_X, TOUCH_UP_ZONE_Y, TOUCH_UP_ZONE_WIDTH, TOUCH_UP_ZONE_HEIGHT)) {
    direction_ = Direction::Up;
    directionAvailable_ = true;
  } else if (pointInZone(x, y, TOUCH_DOWN_ZONE_X, TOUCH_DOWN_ZONE_Y, TOUCH_DOWN_ZONE_WIDTH, TOUCH_DOWN_ZONE_HEIGHT)) {
    direction_ = Direction::Down;
    directionAvailable_ = true;
  } else if (pointInZone(x, y, TOUCH_LEFT_ZONE_X, TOUCH_LEFT_ZONE_Y, TOUCH_LEFT_ZONE_WIDTH, TOUCH_LEFT_ZONE_HEIGHT)) {
    direction_ = Direction::Left;
    directionAvailable_ = true;
  } else if (pointInZone(x, y, TOUCH_RIGHT_ZONE_X, TOUCH_RIGHT_ZONE_Y, TOUCH_RIGHT_ZONE_WIDTH, TOUCH_RIGHT_ZONE_HEIGHT)) {
    direction_ = Direction::Right;
    directionAvailable_ = true;
  } else if (pointInZone(x, y, TOUCH_START_ZONE_X, TOUCH_START_ZONE_Y, TOUCH_START_ZONE_WIDTH, TOUCH_START_ZONE_HEIGHT)) {
    pauseRequested_ = true;
  }
}

bool GameInput::touched() const {
  return touched_;
}

bool GameInput::directionAvailable() const {
  return directionAvailable_;
}

Direction GameInput::direction() const {
  return direction_;
}

bool GameInput::actionAvailable() const {
  return actionAvailable_;
}

ControllerCommand GameInput::action() const {
  return action_;
}

bool GameInput::pauseRequested() const {
  return pauseRequested_;
}

void GameInput::beginTouchController() {
  touchSpi_.begin(TOUCH_SPI_SCLK, TOUCH_SPI_MISO, TOUCH_SPI_MOSI, TOUCH_CS_PIN);
  pinMode(TFT_DISPLAY_CS, OUTPUT);
  digitalWrite(TFT_DISPLAY_CS, HIGH);
  pinMode(TOUCH_IRQ_PIN, INPUT);

  touchCs_ = TOUCH_CS_PIN;
  pinMode(touchCs_, OUTPUT);
  digitalWrite(touchCs_, HIGH);
}

uint16_t GameInput::readPressure() {
  if (touchCs_ < 0) {
    return 0;
  }

  digitalWrite(TFT_DISPLAY_CS, HIGH);
  touchSpi_.beginTransaction(SPISettings(TOUCH_SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
  digitalWrite(touchCs_, LOW);

  int16_t pressure = 0xFFF;
  touchSpi_.transfer(0xB0);
  pressure += touchSpi_.transfer16(0xC0) >> 3;
  pressure -= touchSpi_.transfer16(0x00) >> 3;

  digitalWrite(touchCs_, HIGH);
  touchSpi_.endTransaction();

  if (pressure == 4095 || pressure < 0) {
    return 0;
  }

  return static_cast<uint16_t>(pressure);
}

void GameInput::readRawPoint(uint16_t& rawX, uint16_t& rawY) {
  if (touchCs_ < 0) {
    rawX = 0;
    rawY = 0;
    return;
  }

  digitalWrite(TFT_DISPLAY_CS, HIGH);
  touchSpi_.beginTransaction(SPISettings(TOUCH_SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
  digitalWrite(touchCs_, LOW);

  uint16_t value = 0;

  touchSpi_.transfer(0xD0);
  touchSpi_.transfer(0);
  touchSpi_.transfer(0xD0);
  touchSpi_.transfer(0);
  touchSpi_.transfer(0xD0);
  touchSpi_.transfer(0);
  touchSpi_.transfer(0xD0);

  value = touchSpi_.transfer(0);
  value = value << 5;
  value |= 0x1F & (touchSpi_.transfer(0x90) >> 3);
  rawX = value;

  touchSpi_.transfer(0);
  touchSpi_.transfer(0x90);
  touchSpi_.transfer(0);
  touchSpi_.transfer(0x90);
  touchSpi_.transfer(0);
  touchSpi_.transfer(0x90);

  value = touchSpi_.transfer(0);
  value = value << 5;
  value |= 0x1F & (touchSpi_.transfer(0) >> 3);
  rawY = value;

  digitalWrite(touchCs_, HIGH);
  touchSpi_.endTransaction();
}

bool GameInput::pointInZone(uint16_t x, uint16_t y, int zoneX, int zoneY, int zoneWidth, int zoneHeight) const {
  return x >= zoneX && x < zoneX + zoneWidth &&
         y >= zoneY && y < zoneY + zoneHeight;
}
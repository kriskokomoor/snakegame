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

Input::Input(TFT_eSPI& tft)
    : tft_(tft),
      touchSpi_(VSPI),
      touchCs_(-1),
      touched_(false),
      directionAvailable_(false),
      direction_(Direction::Right) {
}

void Input::begin() {
  beginTouchController();
}

void Input::update() {
  if (DEBUG_DISABLE_TOUCH_POLLING) {
    touched_ = false;
    directionAvailable_ = false;
    return;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  const uint16_t pressure = readPressure();

  touched_ = pressure > TOUCH_THRESHOLD;
  directionAvailable_ = false;

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

  if (pointInButton(x, y, LEFT_BUTTON_X, TOP_BUTTON_Y)) {
    direction_ = Direction::Left;
    directionAvailable_ = true;
  } else if (pointInButton(x, y, LEFT_BUTTON_X, BOTTOM_BUTTON_Y)) {
    direction_ = Direction::Down;
    directionAvailable_ = true;
  } else if (pointInButton(x, y, RIGHT_BUTTON_X, TOP_BUTTON_Y)) {
    direction_ = Direction::Right;
    directionAvailable_ = true;
  } else if (pointInButton(x, y, RIGHT_BUTTON_X, BOTTOM_BUTTON_Y)) {
    direction_ = Direction::Up;
    directionAvailable_ = true;
  }
}

bool Input::touched() const {
  return touched_;
}

bool Input::directionAvailable() const {
  return directionAvailable_;
}

Direction Input::direction() const {
  return direction_;
}

void Input::beginTouchController() {
  touchSpi_.begin(TOUCH_SPI_SCLK, TOUCH_SPI_MISO, TOUCH_SPI_MOSI, TOUCH_CS_PIN);
  pinMode(TFT_DISPLAY_CS, OUTPUT);
  digitalWrite(TFT_DISPLAY_CS, HIGH);
  pinMode(TOUCH_IRQ_PIN, INPUT);

  touchCs_ = TOUCH_CS_PIN;
  pinMode(touchCs_, OUTPUT);
  digitalWrite(touchCs_, HIGH);
}

uint16_t Input::readPressure() {
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

void Input::readRawPoint(uint16_t& rawX, uint16_t& rawY) {
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

bool Input::pointInButton(uint16_t x, uint16_t y, int buttonX, int buttonY) const {
  return x >= buttonX && x < buttonX + BUTTON_SIZE &&
         y >= buttonY && y < buttonY + BUTTON_SIZE;
}

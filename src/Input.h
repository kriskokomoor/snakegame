#pragma once

#include "Game.h"

#include <SPI.h>
#include <TFT_eSPI.h>

class Input {
public:
  explicit Input(TFT_eSPI& tft);

  void begin();
  void update();

  bool touched() const;
  bool directionAvailable() const;
  Direction direction() const;

private:
  void beginTouchController();
  uint16_t readPressure();
  void readRawPoint(uint16_t& rawX, uint16_t& rawY);
  bool pointInButton(uint16_t x, uint16_t y, int buttonX, int buttonY) const;

  TFT_eSPI& tft_;
  SPIClass touchSpi_;
  int touchCs_;
  bool touched_;
  bool directionAvailable_;
  Direction direction_;
};

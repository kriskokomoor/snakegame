#pragma once

#include "Game.h"
#include "controller_manager.h"

#include <SPI.h>
#include <TFT_eSPI.h>

class GameInput {
public:
  explicit GameInput(TFT_eSPI& tft);

  void begin();
  void restoreTouchController();
  void update();
  void suppressTouchUntilRelease();

  bool touched() const;
  bool directionAvailable() const;
  Direction direction() const;
  bool actionAvailable() const;
  ControllerCommand action() const;
  bool pauseRequested() const;

private:
  void beginTouchController();
  uint16_t readPressure();
  void readRawPoint(uint16_t& rawX, uint16_t& rawY);
  bool pointInZone(uint16_t x, uint16_t y, int zoneX, int zoneY, int zoneWidth, int zoneHeight) const;

  TFT_eSPI& tft_;
  SPIClass touchSpi_;
  int touchCs_;
  bool touched_;
  bool directionAvailable_;
  bool actionAvailable_;
  bool pauseRequested_;
  bool suppressTouchUntilRelease_;
  uint8_t touchPressSamples_;
  uint8_t touchReleaseSamples_;
  Direction direction_;
  ControllerCommand action_;
};

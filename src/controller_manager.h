#pragma once

#include <stdint.h>

#include <uni.h>

enum class ControllerCommand : uint8_t {
  NONE,
  UP,
  DOWN,
  LEFT,
  RIGHT,
  ACTION_1,
  ACTION_2,
  START,
  SELECT,
};

struct ControllerSnapshot {
  bool connected;
  bool ready;
  uint8_t connected_count;
  char device_name[HID_MAX_NAME_LEN];
  char model_name[64];
  uint8_t battery;
  uni_gamepad_t gamepad;
  const char* command;
};

class ControllerManager {
 public:
  static void begin();
  static ControllerSnapshot snapshot();
  static ControllerCommand currentCommand();
  static ControllerCommand consumeCommand();
  static bool isConnected();
  static bool isReady();
  static const char* batteryText(uint8_t battery);
  static bool batteryAvailable(uint8_t battery);
  static const char* commandText(ControllerCommand command);

 private:
  static void btstackTask(void* parameter);
  static struct uni_platform* createPlatform();
};

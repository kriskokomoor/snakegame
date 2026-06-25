#include "controller_manager.h"

#include "config.h"

#include <Arduino.h>
#include <bt/uni_bt_allowlist.h>
#include <bt/uni_bt.h>
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr bool kVerboseRawControllerDump = false;
constexpr uint32_t kVerboseRawDumpPeriodMs = 250;
constexpr int32_t kAxisDeadZone = 200;
constexpr uint16_t kAction1Mask = BUTTON_A;
constexpr uint16_t kAction2Mask = BUTTON_B;
constexpr uint16_t k8BitDoVendorId = 0x2dc8;
constexpr uint16_t k8BitDoZero2ProductId = 0x3230;

ControllerSnapshot g_state = {};
uni_hid_device_t* g_active_controller = nullptr;
portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t g_btstack_task = nullptr;
ControllerCommand g_current_command = ControllerCommand::NONE;
ControllerCommand g_pending_command = ControllerCommand::NONE;
bool g_has_pending_command = false;

ControllerCommand semanticCommandFromGamepad(const uni_gamepad_t& gp) {
  if (gp.dpad & DPAD_UP) return ControllerCommand::UP;
  if (gp.dpad & DPAD_DOWN) return ControllerCommand::DOWN;
  if (gp.dpad & DPAD_LEFT) return ControllerCommand::LEFT;
  if (gp.dpad & DPAD_RIGHT) return ControllerCommand::RIGHT;

  const int32_t axis_x = gp.axis_x;
  const int32_t axis_y = gp.axis_y;
  const int32_t abs_x = labs(axis_x);
  const int32_t abs_y = labs(axis_y);
  const bool x_active = abs_x > kAxisDeadZone;
  const bool y_active = abs_y > kAxisDeadZone;
  if (x_active || y_active) {
    if (y_active && (!x_active || abs_y >= abs_x)) {
      return axis_y < 0 ? ControllerCommand::UP : ControllerCommand::DOWN;
    }
    return axis_x < 0 ? ControllerCommand::LEFT : ControllerCommand::RIGHT;
  }

  if (gp.buttons & kAction1Mask) return ControllerCommand::ACTION_1;
  if (gp.buttons & kAction2Mask) return ControllerCommand::ACTION_2;
  if (gp.buttons & BUTTON_X) return ControllerCommand::ACTION_X;
  if (gp.buttons & BUTTON_Y) return ControllerCommand::ACTION_Y;
  if (gp.misc_buttons & MISC_BUTTON_START) return ControllerCommand::START;
  if (gp.misc_buttons & MISC_BUTTON_SELECT) return ControllerCommand::SELECT;
  return ControllerCommand::NONE;
}

bool claimActiveControllerUnlocked(uni_hid_device_t* d) {
  if (g_active_controller == nullptr) {
    g_active_controller = d;
  }
  return g_active_controller == d;
}

void clearCommandsUnlocked() {
  g_current_command = ControllerCommand::NONE;
  g_pending_command = ControllerCommand::NONE;
  g_has_pending_command = false;
  g_state.command = "NONE";
}

void resetControllerStateUnlocked() {
  g_active_controller = nullptr;
  g_state.connected = false;
  g_state.ready = false;
  g_state.connected_count = 0;
  g_state.battery = UNI_CONTROLLER_BATTERY_NOT_AVAILABLE;
  memset(&g_state.gamepad, 0, sizeof(g_state.gamepad));
  snprintf(g_state.device_name, sizeof(g_state.device_name), "%s", "(none)");
  snprintf(g_state.model_name, sizeof(g_state.model_name), "%s", "(none)");
  clearCommandsUnlocked();
}

void copyDeviceInfoUnlocked(uni_hid_device_t* d) {
  const char* model = uni_gamepad_get_model_name(d->controller_type);
  g_state.connected = true;
  snprintf(g_state.device_name, sizeof(g_state.device_name), "%s", d->name[0] ? d->name : "(unnamed)");
  snprintf(g_state.model_name, sizeof(g_state.model_name), "%s", model ? model : "(unknown model)");
  g_state.connected_count = 1;
}

void describeDpad(uint8_t dpad, char* out, size_t len) {
  snprintf(out, len, "%s%s%s%s%s",
           (dpad & DPAD_UP) ? "UP " : "",
           (dpad & DPAD_DOWN) ? "DOWN " : "",
           (dpad & DPAD_LEFT) ? "LEFT " : "",
           (dpad & DPAD_RIGHT) ? "RIGHT " : "",
           dpad ? "" : "none");
}

void describeButtons(uint16_t buttons, char* out, size_t len) {
  snprintf(out, len, "%s%s%s%s%s%s%s%s%s",
           (buttons & BUTTON_A) ? "A " : "",
           (buttons & BUTTON_B) ? "B " : "",
           (buttons & BUTTON_X) ? "X " : "",
           (buttons & BUTTON_Y) ? "Y " : "",
           (buttons & BUTTON_SHOULDER_L) ? "L " : "",
           (buttons & BUTTON_SHOULDER_R) ? "R " : "",
           (buttons & BUTTON_TRIGGER_L) ? "L2 " : "",
           (buttons & BUTTON_TRIGGER_R) ? "R2 " : "",
           buttons ? "" : "none");
}

void describeMisc(uint8_t misc, char* out, size_t len) {
  snprintf(out, len, "%s%s%s%s%s",
           (misc & MISC_BUTTON_START) ? "START " : "",
           (misc & MISC_BUTTON_SELECT) ? "SELECT " : "",
           (misc & MISC_BUTTON_SYSTEM) ? "SYSTEM " : "",
           (misc & MISC_BUTTON_CAPTURE) ? "CAPTURE " : "",
           misc ? "" : "none");
}

void printVerboseRaw(const uni_controller_t& ctl) {
  const uni_gamepad_t& gp = ctl.gamepad;
  Serial.printf("raw250ms: klass=%u battery=%u dpad=0x%02x buttons=0x%04x misc_buttons=0x%02x "
                "axis_x=%ld axis_y=%ld axis_rx=%ld axis_ry=%ld brake=%ld throttle=%ld semantic=%s\n",
                ctl.klass, ctl.battery, gp.dpad, gp.buttons, gp.misc_buttons,
                gp.axis_x, gp.axis_y, gp.axis_rx, gp.axis_ry, gp.brake, gp.throttle,
                ControllerManager::commandText(semanticCommandFromGamepad(gp)));
}

void printCommandEvent(ControllerCommand semantic, const uni_gamepad_t& gp) {
  Serial.printf("EVENT semantic=%s raw_dpad=0x%02x buttons=0x%04x misc=0x%02x axis=(%ld,%ld)\n",
                ControllerManager::commandText(semantic), gp.dpad, gp.buttons, gp.misc_buttons, gp.axis_x,
                gp.axis_y);
}

void maybePrintVerboseRaw(const uni_controller_t& ctl, uint32_t now) {
  if (!kVerboseRawControllerDump) return;

  static uint32_t last_raw_dump_ms = 0;
  if (now - last_raw_dump_ms < kVerboseRawDumpPeriodMs) return;

  printVerboseRaw(ctl);
  last_raw_dump_ms = now;
}

bool is8BitDoZero2(const uni_hid_device_t* d) {
  return d->name[0] != '\0' && strstr(d->name, "8BitDo Zero 2") != nullptr;
}

void applyKnownControllerIdentity(uni_hid_device_t* d) {
  if (!is8BitDoZero2(d)) return;

  if (d->vendor_id == k8BitDoVendorId && d->product_id == k8BitDoZero2ProductId) return;

  Serial.println("Bluepad32: recognized 8BitDo Zero 2 by name; using known VID/PID to avoid fragile SDP query.");
  uni_hid_device_set_vendor_id(d, k8BitDoVendorId);
  uni_hid_device_set_product_id(d, k8BitDoZero2ProductId);
  uni_hid_device_guess_controller_type_from_pid_vid(d);
}

void printStateSummary(const ControllerSnapshot& s) {
  char dpad[40], buttons[64], misc[48];
  describeDpad(s.gamepad.dpad, dpad, sizeof(dpad));
  describeButtons(s.gamepad.buttons, buttons, sizeof(buttons));
  describeMisc(s.gamepad.misc_buttons, misc, sizeof(misc));

  Serial.printf("state: count=%u %s / %s | battery=%s | dpad=%s buttons=%s misc=%s cmd=%s\n",
                s.connected_count, s.model_name, s.device_name,
                ControllerManager::batteryText(s.battery), dpad, buttons, misc, s.command);
}

void maybePrintCommandEvent(const uni_controller_t& ctl, ControllerCommand semantic) {
  const uni_gamepad_t& gp = ctl.gamepad;
  static ControllerCommand last_command = ControllerCommand::NONE;
  static bool have_last_command = false;
  if (have_last_command && last_command == semantic) return;

  printCommandEvent(semantic, gp);
  last_command = semantic;
  have_last_command = true;
}

void platformInit(int, const char**) {}

void platformOnInitComplete() {
  Serial.println("Bluepad32 initialized.");
  Serial.println("Use 8BitDo Zero 2 B+Start / DInput mode, then hold Select for pairing.");
  if (DEBUG_FORGET_BLUETOOTH_KEYS_ON_BOOT) {
    Serial.println("Bluepad32: deleting stored Bluetooth keys before scanning.");
    uni_bt_del_keys_unsafe();
  }
  if (DEBUG_ENABLE_BLUETOOTH_ALLOWLIST) {
    bd_addr_t allowed = {
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[0],
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[1],
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[2],
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[3],
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[4],
        DEBUG_BLUETOOTH_ALLOWLIST_ADDR[5],
    };
    Serial.printf("Bluepad32: allowlisting only %s.\n", bd_addr_to_str(allowed));
    uni_bt_allowlist_remove_all();
    uni_bt_allowlist_add_addr(allowed);
    uni_bt_allowlist_set_enabled(true);
  }
  if (DEBUG_ENABLE_BLUETOOTH_INQUIRY_AUTOCONNECT) {
    Serial.println("Bluepad32: starting inquiry/autoconnect from BTstack thread.");
    uni_bt_start_scanning_and_autoconnect_unsafe();
  } else {
    Serial.println("Bluepad32: inquiry/autoconnect disabled; waiting for controller-initiated connection.");
  }
}

uni_error_t platformOnDeviceDiscovered(bd_addr_t, const char* name, uint16_t cod, uint8_t rssi) {
  Serial.printf("Discovered controller candidate: name=%s cod=0x%04x rssi=%u\n",
                name ? name : "(unnamed)", cod, rssi);
  return UNI_ERROR_SUCCESS;
}

void platformOnDeviceConnected(uni_hid_device_t* d) {
  applyKnownControllerIdentity(d);

  portENTER_CRITICAL(&g_state_mux);
  const bool active = claimActiveControllerUnlocked(d);
  if (active) {
    copyDeviceInfoUnlocked(d);
    g_state.ready = false;
    clearCommandsUnlocked();
  }
  portEXIT_CRITICAL(&g_state_mux);

  if (active) {
    ControllerSnapshot s = ControllerManager::snapshot();
    Serial.printf("Controller connected: addr=%s name=%s count=%u\n", bd_addr_to_str(d->conn.btaddr),
                  d->name[0] ? d->name : "(unnamed)", s.connected_count);
  } else {
    Serial.printf("Ignoring extra controller: addr=%s name=%s\n", bd_addr_to_str(d->conn.btaddr),
                  d->name[0] ? d->name : "(unnamed)");
  }
}

void platformOnDeviceDisconnected(uni_hid_device_t* d) {
  portENTER_CRITICAL(&g_state_mux);
  const bool was_active = (g_active_controller == d);
  if (was_active) resetControllerStateUnlocked();
  const uint8_t connected_count = g_state.connected_count;
  portEXIT_CRITICAL(&g_state_mux);

  Serial.printf("Controller disconnected: addr=%s connected_count=%u\n", bd_addr_to_str(d->conn.btaddr),
                connected_count);
  if (was_active && DEBUG_ENABLE_BLUETOOTH_INQUIRY_AUTOCONNECT) {
    Serial.println("Bluepad32: active controller disconnected; restarting scan/autoconnect.");
    uni_bt_start_scanning_and_autoconnect_unsafe();
  }
}

uni_error_t platformOnDeviceReady(uni_hid_device_t* d) {
  applyKnownControllerIdentity(d);

  portENTER_CRITICAL(&g_state_mux);
  const bool active = claimActiveControllerUnlocked(d);
  if (active) {
    copyDeviceInfoUnlocked(d);
    g_state.ready = true;
    g_state.battery = d->controller.battery;
  }
  portEXIT_CRITICAL(&g_state_mux);

  if (!active) {
    Serial.printf("Extra controller ready but not active: addr=%s name=%s\n", bd_addr_to_str(d->conn.btaddr),
                  d->name[0] ? d->name : "(unnamed)");
    return UNI_ERROR_SUCCESS;
  }

  ControllerSnapshot s = ControllerManager::snapshot();
  Serial.printf("Controller ready: addr=%s model=%s name=%s vid=0x%04x pid=0x%04x battery=%s count=%u\n",
                bd_addr_to_str(d->conn.btaddr), s.model_name, s.device_name, d->vendor_id, d->product_id,
                ControllerManager::batteryText(s.battery), s.connected_count);
  return UNI_ERROR_SUCCESS;
}

void platformOnControllerData(uni_hid_device_t* d, uni_controller_t* ctl) {
  if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;

  const uint32_t now = millis();
  maybePrintVerboseRaw(*ctl, now);
  const ControllerCommand command = semanticCommandFromGamepad(ctl->gamepad);

  portENTER_CRITICAL(&g_state_mux);
  const bool active = claimActiveControllerUnlocked(d);
  if (active) {
    copyDeviceInfoUnlocked(d);
    g_state.ready = true;
    g_state.battery = ctl->battery;
    g_state.gamepad = ctl->gamepad;
    if (command != ControllerCommand::NONE && command != g_current_command) {
      g_current_command = command;
      g_pending_command = command;
      g_has_pending_command = true;
    } else if (command == ControllerCommand::NONE) {
      g_current_command = ControllerCommand::NONE;
    }
    g_state.command = ControllerManager::commandText(g_current_command);
  }
  ControllerSnapshot snapshot = g_state;
  portEXIT_CRITICAL(&g_state_mux);

  if (!active) return;

  maybePrintCommandEvent(*ctl, g_current_command);

  static bool have_last_summary = false;
  static uni_gamepad_t last_gamepad = {};
  static uint8_t last_battery = UNI_CONTROLLER_BATTERY_NOT_AVAILABLE;
  if (kVerboseRawControllerDump &&
      (!have_last_summary || memcmp(&last_gamepad, &snapshot.gamepad, sizeof(snapshot.gamepad)) != 0 ||
       last_battery != snapshot.battery)) {
    printStateSummary(snapshot);
    last_gamepad = snapshot.gamepad;
    last_battery = snapshot.battery;
    have_last_summary = true;
  }
}

void platformOnOobEvent(uni_platform_oob_event_t event, void*) {
  if (event == UNI_PLATFORM_OOB_BLUETOOTH_ENABLED) {
    Serial.println("Bluetooth scanning enabled.");
  }
}

}  // namespace

void ControllerManager::btstackTask(void*) {
  Serial.printf("BTstack service task started on core %d\n", xPortGetCoreID());

  Serial.println("ControllerManager::begin: before uni_platform_set_custom()");
  uni_platform_set_custom(createPlatform());
  Serial.println("ControllerManager::begin: after uni_platform_set_custom()");

  Serial.println("ControllerManager::begin: before btstack_init()");
  const uint8_t btstack_status = btstack_init();
  Serial.printf("ControllerManager::begin: after btstack_init(), return_code=0x%02x (%u)\n", btstack_status,
                btstack_status);
  if (btstack_status != 0) {
    Serial.printf("BTstack init failed: 0x%02x\n", btstack_status);
    g_btstack_task = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  Serial.println("ControllerManager::begin: before uni_init()");
  uni_init(0, nullptr);
  Serial.println("ControllerManager::begin: after uni_init()");

  Serial.println("BTstack service task entering btstack_run_loop_execute(); Bluepad32 callbacks can now run.");
  btstack_run_loop_execute();

  Serial.println("BTstack service task exited btstack_run_loop_execute().");
  g_btstack_task = nullptr;
  vTaskDelete(nullptr);
}

void ControllerManager::begin() {
  if (g_btstack_task != nullptr) {
    Serial.println("ControllerManager::begin: BTstack service task already exists.");
    return;
  }

  portENTER_CRITICAL(&g_state_mux);
  memset(&g_state, 0, sizeof(g_state));
  resetControllerStateUnlocked();
  portEXIT_CRITICAL(&g_state_mux);

  Serial.println("ControllerManager::begin: creating BTstack service task.");
  const BaseType_t created =
      xTaskCreatePinnedToCore(btstackTask, "btstack_service", 8192, nullptr, 12, &g_btstack_task, 0);
  if (created != pdPASS) {
    Serial.println("ControllerManager::begin: failed to create BTstack service task.");
    g_btstack_task = nullptr;
    return;
  }
  Serial.println("ControllerManager::begin: BTstack service task created.");
}

ControllerSnapshot ControllerManager::snapshot() {
  ControllerSnapshot snapshot;
  portENTER_CRITICAL(&g_state_mux);
  snapshot = g_state;
  portEXIT_CRITICAL(&g_state_mux);
  return snapshot;
}

ControllerCommand ControllerManager::currentCommand() {
  ControllerCommand command;
  portENTER_CRITICAL(&g_state_mux);
  command = g_current_command;
  portEXIT_CRITICAL(&g_state_mux);
  return command;
}

ControllerCommand ControllerManager::consumeCommand() {
  ControllerCommand command = ControllerCommand::NONE;
  portENTER_CRITICAL(&g_state_mux);
  if (g_has_pending_command) {
    command = g_pending_command;
    g_pending_command = ControllerCommand::NONE;
    g_has_pending_command = false;
  }
  portEXIT_CRITICAL(&g_state_mux);
  return command;
}

bool ControllerManager::isConnected() {
  bool connected;
  portENTER_CRITICAL(&g_state_mux);
  connected = g_state.connected;
  portEXIT_CRITICAL(&g_state_mux);
  return connected;
}

bool ControllerManager::isReady() {
  bool ready;
  portENTER_CRITICAL(&g_state_mux);
  ready = g_state.ready;
  portEXIT_CRITICAL(&g_state_mux);
  return ready;
}

const char* ControllerManager::batteryText(uint8_t battery) {
  static char text[16];
  if (!batteryAvailable(battery)) return "N/A";
  const uint8_t percent = map(battery, UNI_CONTROLLER_BATTERY_EMPTY, UNI_CONTROLLER_BATTERY_FULL, 0, 100);
  snprintf(text, sizeof(text), "%u%%", percent);
  return text;
}

bool ControllerManager::batteryAvailable(uint8_t battery) {
  return battery != UNI_CONTROLLER_BATTERY_NOT_AVAILABLE;
}

const char* ControllerManager::commandText(ControllerCommand command) {
  switch (command) {
    case ControllerCommand::UP:
      return "UP";
    case ControllerCommand::DOWN:
      return "DOWN";
    case ControllerCommand::LEFT:
      return "LEFT";
    case ControllerCommand::RIGHT:
      return "RIGHT";
    case ControllerCommand::ACTION_1:
      return "ACTION_1";
    case ControllerCommand::ACTION_2:
      return "ACTION_2";
    case ControllerCommand::ACTION_X:
      return "ACTION_X";
    case ControllerCommand::ACTION_Y:
      return "ACTION_Y";
    case ControllerCommand::START:
      return "START";
    case ControllerCommand::SELECT:
      return "SELECT";
    case ControllerCommand::NONE:
    default:
      return "NONE";
  }
}

struct uni_platform* ControllerManager::createPlatform() {
  static uni_platform platform = {
      .name = "CYD 8BitDo Zero 2 Controller Test",
      .init = platformInit,
      .on_init_complete = platformOnInitComplete,
      .on_device_discovered = platformOnDeviceDiscovered,
      .on_device_connected = platformOnDeviceConnected,
      .on_device_disconnected = platformOnDeviceDisconnected,
      .on_device_ready = platformOnDeviceReady,
      .on_gamepad_data = nullptr,
      .on_controller_data = platformOnControllerData,
      .get_property = nullptr,
      .on_oob_event = platformOnOobEvent,
      .device_dump = nullptr,
      .register_console_cmds = nullptr,
  };
  return &platform;
}

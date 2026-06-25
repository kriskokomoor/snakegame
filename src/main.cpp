#include <Arduino.h>
#include <TFT_eSPI.h>
#include <string.h>

#include "Game.h"
#include "Input.h"
#include "ProfileManager.h"
#include "Renderer.h"
#include "config.h"
#include "controller_manager.h"

TFT_eSPI tft;
Game game;
GameInput input(tft);
ProfileManager profiles;
Renderer renderer(tft);

uint32_t lastTickMs = 0;
uint32_t lastBacklightReassertMs = 0;
uint32_t splashStartMs = 0;
bool birthdaySplashActive = true;
uint32_t gameStartTickMs = 0;
uint32_t lastGameUpdateMs = 0;
uint32_t finalGameDurationSec = 0;
uint32_t playerConfirmationUntilMs = 0;
uint32_t firstPlayerHotkeyMs = 0;
uint32_t gameOverInputIgnoreUntilMs = 0;
uint8_t playerHotkeyCount = 0;
bool gameOverRecorded = false;
char playerConfirmation[40] = "";
ControllerCommand playerHotkeySequence[PLAYER_HOTKEY_PRESS_COUNT] = {
    ControllerCommand::ACTION_X,
    ControllerCommand::ACTION_X,
    ControllerCommand::ACTION_X,
};

uint32_t activeGameDurationSec(uint32_t now) {
  if (game.state() == GameState::GameOver) {
    return finalGameDurationSec;
  }
  if (gameStartTickMs == 0) {
    return 0;
  }
  return (now - gameStartTickMs) / 1000;
}

const char* activePlayerConfirmation(uint32_t now) {
  if (playerConfirmationUntilMs != 0 && now < playerConfirmationUntilMs) {
    return playerConfirmation;
  }
  if (playerConfirmation[0] != '\0') {
    playerConfirmation[0] = '\0';
    playerConfirmationUntilMs = 0;
    renderer.invalidate();
  }
  return "";
}

bool handlePlayerHotkey(uint32_t now) {
  if (game.state() == GameState::Play) return false;
  if (game.state() == GameState::GameOver && now < gameOverInputIgnoreUntilMs) return false;
  if (!input.actionAvailable()) return false;

  const ControllerCommand action = input.action();
  if (action != ControllerCommand::ACTION_X) return false;

  if (firstPlayerHotkeyMs == 0 || now - firstPlayerHotkeyMs > PLAYER_HOTKEY_WINDOW_MS) {
    firstPlayerHotkeyMs = now;
    playerHotkeyCount = 0;
  }

  if (action != playerHotkeySequence[playerHotkeyCount]) {
    if (action == playerHotkeySequence[0]) {
      firstPlayerHotkeyMs = now;
      playerHotkeyCount = 1;
    } else {
      firstPlayerHotkeyMs = 0;
      playerHotkeyCount = 0;
    }
    return true;
  }

  ++playerHotkeyCount;
  if (playerHotkeyCount < PLAYER_HOTKEY_PRESS_COUNT) return true;

  profiles.togglePlayer();
  if (!profiles.save()) {
    Serial.println("Profiles: player change was not persisted.");
  }

  const bool dad = strcmp(profiles.currentPlayerName(), "Dad") == 0;
  snprintf(playerConfirmation, sizeof(playerConfirmation), "PLAYER SET TO %s", dad ? "DAD" : "RACHAEL");
  playerConfirmationUntilMs = now + PLAYER_CONFIRMATION_MS;
  firstPlayerHotkeyMs = 0;
  playerHotkeyCount = 0;
  renderer.invalidate();
  Serial.printf("Profiles: current player set to %s\n", profiles.currentPlayerName());
  return true;
}

void markGameStarted(uint32_t now) {
  gameStartTickMs = now;
  lastGameUpdateMs = now;
  finalGameDurationSec = 0;
  gameOverInputIgnoreUntilMs = 0;
  gameOverRecorded = false;
}

void recordGameOverIfNeeded(uint32_t now) {
  if (game.state() != GameState::GameOver || gameOverRecorded) return;

  finalGameDurationSec = gameStartTickMs == 0 ? 0 : (now - gameStartTickMs) / 1000;
  profiles.recordScore(game.score(), finalGameDurationSec, game.snakeLength(), game.currentTickMs());
  gameOverInputIgnoreUntilMs = now + GAME_OVER_INPUT_LOCKOUT_MS;
  gameOverRecorded = true;
  renderer.invalidate();
}

bool highScoresRequested() {
  return input.actionAvailable() && input.action() == ControllerCommand::SELECT;
}

bool highScoresDismissed() {
  if (input.pauseRequested()) return true;
  if (!input.actionAvailable()) return false;
  const ControllerCommand action = input.action();
  return action == ControllerCommand::SELECT || action == ControllerCommand::ACTION_2;
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  renderer.begin();
  renderer.beginSd();               // SD on VSPI (pins 18/19/23) – must be before input.begin()
  game.begin();
  renderer.drawBirthdaySplash(false, 0); // loads BMP once; marks sdAvailable_=false when done
  profiles.begin();
  input.begin();  // reconfigures VSPI for touch (pins 25/32/39); also starts controller manager

  lastTickMs = millis();
  lastBacklightReassertMs = lastTickMs;
  splashStartMs = lastTickMs;
  
  Serial.println("=== Snake Game Started ===");
  Serial.println("Waiting for controller initialization...");
}

void loop() {
  const uint32_t now = millis();

  if (DEBUG_REASSERT_BACKLIGHT && now - lastBacklightReassertMs >= DEBUG_BACKLIGHT_REASSERT_MS) {
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    lastBacklightReassertMs = now;
  }

  input.update();

  // Birthday splash screen with controller wait
  if (birthdaySplashActive) {
    uint32_t birthdaySplashElapsedMs = now - splashStartMs;
    bool controllerReady = ControllerManager::isReady();

    if (!controllerReady && birthdaySplashElapsedMs < BIRTHDAY_SPLASH_TIMEOUT_MS) {
      renderer.drawBirthdaySplash(controllerReady, birthdaySplashElapsedMs);
      delay(50); // yield to IDLE so watchdog doesn't fire during splash
      return;
    }
    birthdaySplashActive = false;
    input.suppressTouchUntilRelease();
    game.setState(GameState::Ready);
    renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
  }

  bool controllerReady = ControllerManager::isReady();

  if (handlePlayerHotkey(now)) {
    if (!DEBUG_DISABLE_GAME_RENDER) {
      renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
    }
    delay(1);
    return;
  }
  
  // Handle game state transitions
  if (game.state() == GameState::Ready) {
    if (highScoresRequested()) {
      game.setState(GameState::HighScores);
      renderer.invalidate();
      Serial.println("GAME: State changed to HIGH_SCORES");
    }
    // Wait for input to start game
    else if (input.directionAvailable()) {
      game.restart(input.direction());
      game.setState(GameState::Play);
      markGameStarted(now);
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
      Serial.println("GAME: State changed to PLAY");
    } else if (input.pauseRequested()) {
      // Can use action button to start as well
      game.restart(Direction::Right);
      game.setState(GameState::Play);
      markGameStarted(now);
       if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
      Serial.println("GAME: State changed to PLAY (via action button)");
    }
  } 
  else if (game.state() == GameState::GameOver) {
    if (now < gameOverInputIgnoreUntilMs) {
      // Keep draining input events, but do not let a stale command restart immediately.
    } else if (highScoresRequested()) {
      game.setState(GameState::HighScores);
      renderer.invalidate();
      Serial.println("GAME: State changed to HIGH_SCORES");
    }
    // Wait for input to restart
    else if (input.directionAvailable()) {
      game.restart(input.direction());
      game.setState(GameState::Play);
      markGameStarted(now);
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
      Serial.println("GAME: State changed to PLAY (restart)");
    } else if (input.pauseRequested() ||
               (input.actionAvailable() && input.action() == ControllerCommand::ACTION_2)) {
      // Can use action button to restart as well
      game.restart(Direction::Right);
      game.setState(GameState::Play);
      markGameStarted(now);
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
      Serial.println("GAME: State changed to PLAY (restart via action)");
    }
  }
  else if (game.state() == GameState::Play) {
    // Handle pause request
    if (input.pauseRequested()) {
      game.togglePause();
      if (game.state() == GameState::Pause) {
        Serial.println("GAME: State changed to PAUSE");
      } else {
        Serial.println("GAME: State changed to PLAY (resumed)");
      }
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
    }
    // Handle direction input
    else if (input.directionAvailable()) {
      game.setDirection(input.direction());
    }
    // Update game on tick
    else if (now - lastGameUpdateMs >= game.currentTickMs()) {
      if (game.update()) {
        recordGameOverIfNeeded(now);
        if (!DEBUG_DISABLE_GAME_RENDER) {
          renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
        }
      }
      lastGameUpdateMs = now;
    }
  }
  else if (game.state() == GameState::Pause) {
    if (highScoresRequested()) {
      game.setState(GameState::HighScores);
      renderer.invalidate();
      Serial.println("GAME: State changed to HIGH_SCORES");
    }
    // Handle resume request
    else if (input.pauseRequested()) {
      game.togglePause();
      Serial.println("GAME: State changed to PLAY (resumed)");
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
      }
    }
  }
  else if (game.state() == GameState::HighScores) {
    if (highScoresDismissed()) {
      game.setState(GameState::Ready);
      renderer.invalidate();
      Serial.println("GAME: State changed to READY");
    }
  }

  recordGameOverIfNeeded(now);

  // Regular rendering update
  if (!DEBUG_DISABLE_GAME_RENDER) {
    renderer.draw(game, controllerReady, profiles, activeGameDurationSec(now), activePlayerConfirmation(now));
  }

  delay(1);  // yield to IDLE1 so the task watchdog doesn't fire
}

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Game.h"
#include "Input.h"
#include "Renderer.h"
#include "config.h"
#include "controller_manager.h"

TFT_eSPI tft;
Game game;
GameInput input(tft);
Renderer renderer(tft);

uint32_t lastTickMs = 0;
uint32_t lastBacklightReassertMs = 0;
uint32_t splashStartMs = 0;
bool birthdaySplashActive = true;
uint32_t gameStartTickMs = 0;
uint32_t lastGameUpdateMs = 0;

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  renderer.begin();
  input.begin();  // This also starts the controller manager
  game.begin();
  renderer.drawBirthdaySplash(false, 0); // Start with the birthday splash

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

    if (!controllerReady && birthdaySplashElapsedMs < BIRTHDAY_SPLASH_TIMEOUT_MS && !input.touched() && !input.directionAvailable()) {
      renderer.drawBirthdaySplash(controllerReady, birthdaySplashElapsedMs);
      delay(50); // yield to IDLE so watchdog doesn't fire during splash
      return;
    }
    birthdaySplashActive = false;
    game.setState(GameState::Ready);
    renderer.draw(game, ControllerManager::isReady());
  }

  bool controllerReady = ControllerManager::isReady();
  
  // Handle game state transitions
  if (game.state() == GameState::Ready) {
    // Wait for input to start game
    if (input.directionAvailable()) {
      game.restart(input.direction());
      game.setState(GameState::Play);
      gameStartTickMs = now;
      lastGameUpdateMs = now;
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady);
      }
      Serial.println("GAME: State changed to PLAY");
    } else if (input.pauseRequested()) {
      // Can use action button to start as well
      game.restart(Direction::Right);
      game.setState(GameState::Play);
      gameStartTickMs = now;
       if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady);
      }
      lastGameUpdateMs = now;
      Serial.println("GAME: State changed to PLAY (via action button)");
    }
  } 
  else if (game.state() == GameState::GameOver) {
    // Wait for input to restart
    if (input.directionAvailable()) {
      game.restart(input.direction());
      game.setState(GameState::Play);
      gameStartTickMs = now;
      lastGameUpdateMs = now;
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady);
      }
      Serial.println("GAME: State changed to PLAY (restart)");
    } else if (input.pauseRequested() || input.actionAvailable()) {
      // Can use action button to restart as well
      game.restart(Direction::Right);
      game.setState(GameState::Play);
      gameStartTickMs = now;
      lastGameUpdateMs = now;
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady);
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
        renderer.draw(game, controllerReady);
      }
    }
    // Handle direction input
    else if (input.directionAvailable()) {
      game.setDirection(input.direction());
    }
    // Update game on tick
    else if (now - lastGameUpdateMs >= game.currentTickMs()) {
      if (game.update()) {
        if (!DEBUG_DISABLE_GAME_RENDER) {
          renderer.draw(game, controllerReady);
        }
      }
      lastGameUpdateMs = now;
    }
  }
  else if (game.state() == GameState::Pause) {
    // Handle resume request
    if (input.pauseRequested()) {
      game.togglePause();
      Serial.println("GAME: State changed to PLAY (resumed)");
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game, controllerReady);
      }
    }
  }

  // Regular rendering update
  if (!DEBUG_DISABLE_GAME_RENDER) {
    renderer.draw(game, controllerReady);
  }

  delay(1);  // yield to IDLE1 so the task watchdog doesn't fire
}

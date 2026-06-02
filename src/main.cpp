#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Game.h"
#include "Input.h"
#include "Renderer.h"
#include "config.h"

TFT_eSPI tft;
Game game;
Input input(tft);
Renderer renderer(tft);

uint32_t lastTickMs = 0;
uint32_t lastBacklightReassertMs = 0;
uint32_t splashStartMs = 0;
bool gameStarted = false;
bool splashActive = true;

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, HIGH);

  renderer.begin();
  input.begin();
  game.begin();
  renderer.drawStartupSplash();

  lastTickMs = millis();
  lastBacklightReassertMs = lastTickMs;
  splashStartMs = lastTickMs;
}

void loop() {
  const uint32_t now = millis();

  if (DEBUG_REASSERT_BACKLIGHT && now - lastBacklightReassertMs >= DEBUG_BACKLIGHT_REASSERT_MS) {
    digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
    lastBacklightReassertMs = now;
  }

  input.update();

  if (splashActive) {
    if (now - splashStartMs < STARTUP_SPLASH_MS) {
      return;
    }

    splashActive = false;
    if (!DEBUG_DISABLE_GAME_RENDER) {
      renderer.draw(game);
    }
  }

  if (game.isGameOver()) {
    if (input.directionAvailable()) {
      game.restart(input.direction());
      gameStarted = false;
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game);
      }
      gameStarted = true;
      lastTickMs = now;
    }
    return;
  }

  if (input.directionAvailable()) {
    if (!gameStarted) {
      game.restart(input.direction());
      if (!DEBUG_DISABLE_GAME_RENDER) {
        renderer.draw(game);
      }
      gameStarted = true;
      lastTickMs = now;
      return;
    }

    game.setDirection(input.direction());
  }

  if (!gameStarted) {
    return;
  }

  if (now - lastTickMs >= game.currentTickMs()) {
    lastTickMs += game.currentTickMs();
    game.update();
    if (!DEBUG_DISABLE_GAME_RENDER) {
      renderer.draw(game);
    }
  }
}

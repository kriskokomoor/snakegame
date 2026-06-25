#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>

constexpr uint8_t MAX_HIGH_SCORES = 10;
constexpr uint8_t PLAYER_NAME_MAX_LEN = 12;

struct HighScoreEntry {
  char player[PLAYER_NAME_MAX_LEN];
  int score;
  uint32_t durationSeconds;
  int snakeLength;
  uint32_t speedMs;
};

class ProfileManager {
public:
  void begin();
  const char* currentPlayerName() const;
  const char* compactPlayerName() const;
  void togglePlayer();
  bool save();
  bool storageAvailable() const;
  bool sdAvailable() const;

  bool recordScore(int score, uint32_t durationSeconds, int snakeLength, uint32_t speedMs);
  uint8_t scoreCount() const;
  const HighScoreEntry& scoreAt(uint8_t index) const;

  static void formatDuration(uint32_t durationSeconds, char* out, size_t len);

private:
  bool load();
  void setCurrentPlayer(const char* player);
  void writeDefaultState();
  bool validateLoadedScores();
  void sortScores();
  bool isKnownPlayer(const char* player) const;

  bool currentPlayerIsDad_ = false;
  bool storageAvailable_ = false;
  Preferences preferences_;
  HighScoreEntry scores_[MAX_HIGH_SCORES] = {};
  uint8_t scoreCount_ = 0;
};

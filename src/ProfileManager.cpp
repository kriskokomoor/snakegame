#include "ProfileManager.h"

#include "config.h"

#include <Arduino.h>
#include <string.h>

namespace {
constexpr const char* kPreferencesNamespace = "snakeProfiles";
constexpr const char* kVersionKey = "version";
constexpr const char* kCurrentPlayerKey = "currentDad";
constexpr const char* kScoreCountKey = "scoreCount";
constexpr const char* kScoresKey = "scores";
constexpr uint16_t kStorageVersion = 1;
constexpr const char* kPlayerDad = "Dad";
constexpr const char* kPlayerRachael = "Rachael";

void copyPlayer(char* dest, const char* player) {
  snprintf(dest, PLAYER_NAME_MAX_LEN, "%s", player);
}

bool scoreComesBefore(const HighScoreEntry& a, const HighScoreEntry& b) {
  if (a.score != b.score) return a.score > b.score;
  if (a.durationSeconds != b.durationSeconds) return a.durationSeconds < b.durationSeconds;
  return a.snakeLength > b.snakeLength;
}
}

void ProfileManager::begin() {
  writeDefaultState();
  storageAvailable_ = preferences_.begin(kPreferencesNamespace, false);
  if (!storageAvailable_) {
    Serial.println("Profiles: NVS unavailable; using default player and in-memory scores.");
    return;
  }

  if (!load()) {
    Serial.println("Profiles: no valid saved profile data; creating defaults.");
    save();
  }
}

const char* ProfileManager::currentPlayerName() const {
  return currentPlayerIsDad_ ? kPlayerDad : kPlayerRachael;
}

const char* ProfileManager::compactPlayerName() const {
  return currentPlayerIsDad_ ? "DAD" : "RACH";
}

void ProfileManager::togglePlayer() {
  currentPlayerIsDad_ = !currentPlayerIsDad_;
}

bool ProfileManager::save() {
  if (!storageAvailable_) return false;

  bool ok = preferences_.putUShort(kVersionKey, kStorageVersion) > 0;
  ok = preferences_.putBool(kCurrentPlayerKey, currentPlayerIsDad_) && ok;
  ok = preferences_.putUChar(kScoreCountKey, scoreCount_) > 0 && ok;
  ok = preferences_.putBytes(kScoresKey, scores_, sizeof(scores_)) == sizeof(scores_) && ok;
  if (!ok) {
    Serial.println("Profiles: failed to save profile data to NVS.");
  }
  return ok;
}

bool ProfileManager::storageAvailable() const {
  return storageAvailable_;
}

bool ProfileManager::sdAvailable() const {
  return storageAvailable();
}

bool ProfileManager::recordScore(int score, uint32_t durationSeconds, int snakeLength, uint32_t speedMs) {
  HighScoreEntry entry = {};
  copyPlayer(entry.player, currentPlayerName());
  entry.score = score;
  entry.durationSeconds = durationSeconds;
  entry.snakeLength = snakeLength;
  entry.speedMs = speedMs;

  if (scoreCount_ < MAX_HIGH_SCORES) {
    scores_[scoreCount_++] = entry;
  } else if (scoreComesBefore(entry, scores_[MAX_HIGH_SCORES - 1])) {
    scores_[MAX_HIGH_SCORES - 1] = entry;
  } else {
    save();
    return false;
  }

  sortScores();
  save();
  return true;
}

uint8_t ProfileManager::scoreCount() const {
  return scoreCount_;
}

const HighScoreEntry& ProfileManager::scoreAt(uint8_t index) const {
  if (index >= scoreCount_) index = 0;
  return scores_[index];
}

void ProfileManager::formatDuration(uint32_t durationSeconds, char* out, size_t len) {
  snprintf(out, len, "%02lu:%02lu",
           static_cast<unsigned long>(durationSeconds / 60),
           static_cast<unsigned long>(durationSeconds % 60));
}

bool ProfileManager::load() {
  if (!storageAvailable_) return false;
  if (!preferences_.isKey(kVersionKey)) return false;

  const uint16_t version = preferences_.getUShort(kVersionKey, 0);
  if (version != kStorageVersion) {
    Serial.printf("Profiles: unsupported storage version %u.\n", version);
    return false;
  }

  currentPlayerIsDad_ = preferences_.getBool(kCurrentPlayerKey, false);
  scoreCount_ = preferences_.getUChar(kScoreCountKey, 0);
  memset(scores_, 0, sizeof(scores_));

  const size_t bytesRead = preferences_.getBytes(kScoresKey, scores_, sizeof(scores_));
  if (bytesRead != sizeof(scores_)) {
    Serial.println("Profiles: saved score data missing or truncated.");
    return false;
  }

  if (!validateLoadedScores()) {
    Serial.println("Profiles: saved score data failed validation.");
    return false;
  }

  sortScores();
  return true;
}

void ProfileManager::setCurrentPlayer(const char* player) {
  currentPlayerIsDad_ = strcmp(player, kPlayerDad) == 0;
}

void ProfileManager::writeDefaultState() {
  currentPlayerIsDad_ = false;
  scoreCount_ = 0;
  memset(scores_, 0, sizeof(scores_));
}

bool ProfileManager::validateLoadedScores() {
  if (scoreCount_ > MAX_HIGH_SCORES) {
    return false;
  }

  for (uint8_t i = 0; i < scoreCount_; ++i) {
    HighScoreEntry& score = scores_[i];
    score.player[PLAYER_NAME_MAX_LEN - 1] = '\0';
    if (!isKnownPlayer(score.player)) {
      return false;
    }
    if (score.score < 0 || score.snakeLength < 0 || score.snakeLength > MAX_SNAKE_LENGTH) {
      return false;
    }
  }

  return true;
}

void ProfileManager::sortScores() {
  for (uint8_t i = 0; i < scoreCount_; ++i) {
    for (uint8_t j = i + 1; j < scoreCount_; ++j) {
      if (scoreComesBefore(scores_[j], scores_[i])) {
        HighScoreEntry tmp = scores_[i];
        scores_[i] = scores_[j];
        scores_[j] = tmp;
      }
    }
  }
}

bool ProfileManager::isKnownPlayer(const char* player) const {
  return strcmp(player, kPlayerDad) == 0 || strcmp(player, kPlayerRachael) == 0;
}

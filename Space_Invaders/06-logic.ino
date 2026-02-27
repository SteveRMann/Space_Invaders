// --------------------------------------------------------------------------
// 6. LOGIC & CONFIGURATION (MUST BE BEFORE SETUP)
// --------------------------------------------------------------------------
void saveHighscores() {
  preferences.begin("game", false);
  preferences.putInt((currentProfilePrefix + "hs").c_str(), highScore);
  preferences.putInt((currentProfilePrefix + "l1").c_str(), lastGames[0]);
  preferences.putInt((currentProfilePrefix + "l2").c_str(), lastGames[1]);
  preferences.putInt((currentProfilePrefix + "l3").c_str(), lastGames[2]);

  // Save Stats
  preferences.putULong("st_shots", stat_totalShots);
  preferences.putULong("st_kills", stat_totalKills);
  preferences.end();
}

void loadHighscores() {
  preferences.begin("game", true);
  highScore = preferences.getInt((currentProfilePrefix + "hs").c_str(), 0);
  lastGames[0] = preferences.getInt((currentProfilePrefix + "l1").c_str(), 0);
  lastGames[1] = preferences.getInt((currentProfilePrefix + "l2").c_str(), 0);
  lastGames[2] = preferences.getInt((currentProfilePrefix + "l3").c_str(), 0);

  stat_totalShots = preferences.getULong("st_shots", 0);
  stat_totalKills = preferences.getULong("st_kills", 0);
  preferences.end();
}

void registerGameEnd(int finalScore) {
  lastGames[2] = lastGames[1];
  lastGames[1] = lastGames[0];
  lastGames[0] = finalScore;
  if (finalScore > highScore) highScore = finalScore;
  saveHighscores();
}

void triggerBaseDestruction() {
  playSound(EVT_LOSE);
  registerGameEnd(currentScore);
  currentState = STATE_BASE_DESTROYED;
  stateTimer = millis();
}

void calculateLevelScore() {
  unsigned long duration = millis() - levelStartTime;
  int entityCount = 0;
  if (levels[currentLevel].bossType == 0) entityCount = levels[currentLevel].length;
  else {
    if (levels[currentLevel].bossType == 1) entityCount = 9 * boss1Cfg.hpPerLed;
    else if (levels[currentLevel].bossType == 2) entityCount = 9 * boss2Cfg.hpPerLed;
    else if (levels[currentLevel].bossType == 3) entityCount = 15 * boss3Cfg.hpPerLed;
  }
  int levelMultiplier = currentLevel;
  int basePoints = entityCount * 100 * levelMultiplier;
  unsigned long targetTime = 0;
  if (levels[currentLevel].bossType == 2) targetTime = 38000;
  else {
    unsigned long travelTime = config_num_leds * 15;
    unsigned long processingTime = entityCount * 300;
    targetTime = 3000 + travelTime + processingTime;
  }
  int timeBonus = 0;
  if (levels[currentLevel].bossType == 3) timeBonus = basePoints;
  else {
    if (duration <= targetTime) timeBonus = basePoints;
    else {
      float ratio = (float)targetTime / (float)duration;
      timeBonus = (int)(basePoints * ratio);
    }
  }
  levelAchievedScore = basePoints + timeBonus;
  levelMaxPossibleScore = basePoints * 2;
  currentScore += levelAchievedScore;
}

void checkWinCondition() {
  bool won = false;
  if (currentState == STATE_PLAYING && enemies.empty()) won = true;
  if (currentState == STATE_BOSS_PLAYING && bossSegments.empty()) won = true;
  if (won) {
    calculateLevelScore();
    if (currentLevel >= 10) {
      playSound(EVT_WIN);
      registerGameEnd(currentScore);
      currentState = STATE_GAME_FINISHED;
    } else {
      playSound(EVT_WIN);
      currentState = STATE_LEVEL_COMPLETED;
      stateTimer = millis();
    }
  }
}

void startLevelIntro(int level) {
  playSound(EVT_START);
  if (level == config_start_level) {
    currentScore = 0;
    stat_lastGameShots = 0;  // Reset session shots
  }
  currentLevel = level;
  currentState = STATE_INTRO;
  stateTimer = millis();
  FastLED.clear();
  for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(10, 10, 10);
  CRGB barColor = levels[level].bossType > 0 ? col_c2 : col_c3;
  int center = config_num_leds / 2;
  int totalWidth = (level * 6) + ((level - 1) * 4);
  int startPos = center - (totalWidth / 2);
  if (startPos < 0) startPos = 0;
  int cursor = startPos;
  for (int i = 0; i < level; i++) {
    for (int k = 0; k < 6; k++) {
      if (cursor < config_num_leds) leds[cursor + ledStartOffset] = barColor;
      cursor++;
    }
    cursor += 4;
  }
  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();
}

void drawLevelIntro(int level) {
  FastLED.clear();
  for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(5, 5, 5);
  CRGB barColor = levels[level].bossType > 0 ? col_c2 : col_c3;
  int center = config_num_leds / 2;
  int totalWidth = (level * 6) + ((level - 1) * 4);
  int startPos = center - (totalWidth / 2);
  if (startPos < 0) startPos = 0;
  int cursor = startPos;
  for (int i = 0; i < level; i++) {
    for (int k = 0; k < 6; k++) {
      if (cursor < config_num_leds) leds[cursor + ledStartOffset] = barColor;
      cursor++;
    }
    cursor += 4;
  }
  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();
}

void updateLevelIntro() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed > 2000 && elapsed < 4000) {
    if ((elapsed / 250) % 2 == 0) drawLevelIntro(currentLevel);
    else {
      FastLED.clear();
      if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
      FastLED.show();
    }
  } else if (elapsed <= 2000) {
    drawLevelIntro(currentLevel);
  }

  if (elapsed >= 4000) {
    uint8_t bright = map(config_brightness_pct, 10, 100, 25, 255);
    FastLED.setBrightness(bright);
    levelStartTime = millis();
    if (levels[currentLevel].bossType > 0) {
      currentBossType = levels[currentLevel].bossType;
      bossSegments.clear();
      enemies.clear();
      shots.clear();
      bossProjectiles.clear();
      enemyFrontIndex = (float)config_num_leds - 1.0;
      if (currentBossType == 1) {
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 1, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        for (int i = 0; i < 3; i++) bossSegments.push_back({ 3, boss1Cfg.hpPerLed, boss1Cfg.hpPerLed, true, 0 });
        bossActionTimer = millis();
      } else if (currentBossType == 2) {
        for (int i = 0; i < 9; i++) bossSegments.push_back({ 0, boss2Cfg.hpPerLed, boss2Cfg.hpPerLed, false, i });
        boss2Section = 0;
        boss2State = B2_MOVE;
        markerPos[0] = (int)(config_num_leds * (boss2Cfg.m1 / 100.0));
        markerPos[1] = (int)(config_num_leds * (boss2Cfg.m2 / 100.0));
        markerPos[2] = (int)(config_num_leds * (boss2Cfg.m3 / 100.0));
      } else if (currentBossType == 3) {
        for (int i = 0; i < 15; i++) {
          int mixColor = random(4, 7);
          bossSegments.push_back({ mixColor, boss3Cfg.hpPerLed, boss3Cfg.hpPerLed, true, i });
        }
        boss3State = B3_MOVE;
        boss3PhaseIndex = 0;
        boss3Markers[0] = (int)(config_num_leds * 0.66);
        boss3Markers[1] = (int)(config_num_leds * 0.50);
        bossActionTimer = millis();
      }
      currentState = STATE_BOSS_PLAYING;
    } else {
      enemies.clear();
      shots.clear();
      bossProjectiles.clear();
      int count = levels[currentLevel].length;
      if (count <= 0) count = 10;

      for (int i = 0; i < count; i++) {
        enemies.push_back({ (int)random(1, 4), 0.0 });
      }
      enemyFrontIndex = (float)config_num_leds - 1.0;
      currentState = STATE_PLAYING;
    }
  }
}

void updateLevelCompletedAnim() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < 1000) {
    fill_solid(leds, config_num_leds + ledStartOffset, col_c3);
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  } else if (elapsed < 5000) {
    FastLED.clear();
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    float pct = (float)levelAchievedScore / (float)levelMaxPossibleScore;
    if (pct > 1.0) pct = 1.0;
    int fillLeds = (int)(config_num_leds * pct);
    for (int i = 0; i < fillLeds; i++) leds[i + ledStartOffset] = CRGB(80, 60, 0);
    for (int i = fillLeds; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB(20, 0, 0);
  } else {
    ///startLevelIntro(currentLevel + 1);
  }
  FastLED.show();
}

void updateBaseDestroyedAnim() {
  unsigned long elapsed = millis() - stateTimer;
  if (elapsed < 2000) {
    CRGB c = (elapsed / 100) % 2 == 0 ? col_c2 : CRGB::White;
    for (int i = 0; i < config_homebase_size; i++) {
      if (i + ledStartOffset < config_num_leds) leds[i + ledStartOffset] = c;
    }
    for (int i = config_homebase_size; i < config_num_leds; i++) {
      leds[i + ledStartOffset].nscale8(240);
    }
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
  } else {
    currentState = STATE_GAMEOVER;
  }
}

void moveBossProjectiles(float speed) {
  static unsigned long lastMove = 0;
  float step = (float)speed / 60.0;
  if (step < 0.1) step = 0.1;

  for (int i = bossProjectiles.size() - 1; i >= 0; i--) {
    bossProjectiles[i].pos -= step;
    if (bossProjectiles[i].pos < config_homebase_size) {
      triggerBaseDestruction();
    }
  }
}

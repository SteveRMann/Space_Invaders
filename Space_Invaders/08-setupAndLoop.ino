// --------------------------------------------------------------------------
// 8. SETUP & LOOP
// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("SETUP: start");
  pinMode(PIN_BTN_BLUE, INPUT_PULLUP);
  pinMode(PIN_BTN_RED, INPUT_PULLUP);
  pinMode(PIN_BTN_GREEN, INPUT_PULLUP);
  pinMode(PIN_BTN_WHITE, INPUT_PULLUP);

  setupDefaultConfig();
  preferences.begin("game", true);
  int storedVer = preferences.getInt("version", 0);
  preferences.end();
  if (storedVer < CONFIG_VERSION) {
    performFactoryReset();
    ESP.restart();
  }
  preferences.begin("game", true);
  currentProfilePrefix = preferences.getString("act_prof", "def_");
  preferences.end();
  loadConfig(currentProfilePrefix);
  loadHighscores();
  loadSounds();
  loadColors();

  Serial.println("SETUP: FastLED");
  FastLED.addLeds<LED_TYPE, PIN_LED_DATA, COLOR_ORDER>(leds, config_num_leds + 1);
  FastLED.setBrightness(map(config_brightness_pct, 10, 100, 25, 255));
  FastLED.setDither(0);

  // 4. POWER PROTECTION
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500);  // 2.5 Amps limit

  if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
  FastLED.show();

  Serial.println("SETUP: before 'xQueueCreate()'");
  // --- AUDIO SETUP CORE 0 ---
  ///audioQueue = xQueueCreate(10, sizeof(SoundEvent));
  ///Serial.print("audioQueue handle: ");
  ///Serial.println((uint32_t)audioQueue, HEX);

  ///xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 1, &audioTaskHandle, 0);

  Serial.println("SETUP: before 'Web Routes'");
  WiFi.mode(WIFI_OFF);

  // WEB ROUTES
  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });
  server.on("/save", handleSave);
  server.on("/loadprofile", handleProfileSwitch);
  server.on("/reset", handleReset);
  server.on("/sounds", []() {
    server.send(200, "text/html", getSoundHTML());
  });
  server.on("/savesounds", handleSaveSounds);
  server.on("/colors", []() {
    server.send(200, "text/html", getColorHTML());
  });
  server.on("/savecolors", handleSaveColors);

  Serial.println("SETUP: before 'OTA Handlers'");
  // OTA HANDLERS
  server.on("/update", HTTP_GET, []() {
    server.send(200, "text/html", getUpdateHTML());
  });
  server.on(
    "/update", HTTP_POST, []() {
      server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED" : "UPDATE SUCCESS! RESTARTING...");
      ESP.restart();
    },
    []() {
      HTTPUpload &upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) Serial.printf("Update Success: %u\n", upload.totalSize);
        else Update.printError(Serial);
      }
    });

  startLevelIntro(config_start_level);

  Serial.println("SETUP: end");
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopTime < FRAME_DELAY) return;
  lastLoopTime = now;

  if (wifiMode) {
    server.handleClient();
    static unsigned long lastWifiLedUpdate = 0;
    if (now - lastWifiLedUpdate > 2000) {
      lastWifiLedUpdate = now;
      FastLED.clear();
      for (int i = 0; i <= config_num_leds; i += 2) leds[i + ledStartOffset] = CRGB::Blue;
      FastLED.show();
    }
    // EXIT WIFI MODE
    if (digitalRead(PIN_BTN_WHITE) == LOW) {
      delay(200);
      ESP.restart();
    }
    return;
  }

  // 6. SETUP MODE FEEDBACK
  if (digitalRead(PIN_BTN_WHITE) == LOW) {
    if (!btnWhiteHeld) {
      btnWhiteHeld = true;
      btnWhitePressTime = now;
    } else {
      // Feedback if held > 3s
      if (now - btnWhitePressTime > 3000) {
        FastLED.clear();
        // Blink Blue/Black every 250ms
        if ((now / 250) % 2 == 0) {
          for (int i = 0; i < config_num_leds; i += 2) leds[i + ledStartOffset] = CRGB::Blue;
        }
        FastLED.show();
        return;  // Pause Game Loop while holding
      }
    }
  } else {
    // RELEASED
    if (btnWhiteHeld) {
      unsigned long holdTime = now - btnWhitePressTime;
      btnWhiteHeld = false;
      if (holdTime > 3000) {
        wifiMode = true;
        enableWiFi();
        return;
      } else if (holdTime < 1000) {
        ////startLevelIntro(config_start_level);
      }
    }
  }

  if (currentState == STATE_LEVEL_COMPLETED) {
    updateLevelCompletedAnim();
    return;
  }
  if (currentState == STATE_BASE_DESTROYED) {
    updateBaseDestroyedAnim();
    return;
  }
  if (currentState == STATE_GAME_FINISHED) {
    for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CHSV((now / 10) + (i * 5), 255, 255);
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
    return;
  }
  if (currentState == STATE_GAMEOVER) {
    for (int i = 0; i < config_num_leds; i++) leds[i + ledStartOffset] = CRGB::Red;
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
    return;
  }
  if (currentState == STATE_INTRO) {
    updateLevelIntro();
    return;
  }

  if (currentState == STATE_PLAYING || currentState == STATE_BOSS_PLAYING) {
    bool b = (digitalRead(PIN_BTN_BLUE) == LOW);
    bool r = (digitalRead(PIN_BTN_RED) == LOW);
    bool g = (digitalRead(PIN_BTN_GREEN) == LOW);
    bool isAnyBtnPressed = (b || r || g);

    if (!isAnyBtnPressed) {
      buttonsReleased = true;
      isWaitingForCombo = false;
    }

    // INPUT HANDLING
    if (currentBossType == 3) {
      if (isAnyBtnPressed && buttonsReleased && !isWaitingForCombo && (now - lastFireTime > FIRE_COOLDOWN)) {
        isWaitingForCombo = true;
        comboTimer = now;
      }
      if (isWaitingForCombo && (now - comboTimer >= INPUT_BUFFER_MS)) {
        int c = 0;
        b = (digitalRead(PIN_BTN_BLUE) == LOW);
        r = (digitalRead(PIN_BTN_RED) == LOW);
        g = (digitalRead(PIN_BTN_GREEN) == LOW);
        if (r && g && b) c = 7;
        else if (r && g) c = 4;
        else if (r && b) c = 5;
        else if (g && b) c = 6;
        else if (b) c = 1;
        else if (r) c = 2;
        else if (g) c = 3;
        if (c > 0) {
          shots.push_back({ 0.0, c });
          stat_totalShots++;
          stat_lastGameShots++;  // STATS UPDATE
          saveHighscores();      // Persist shots? Maybe too often. Lets save at end of game/level.
          lastFireTime = now;
          playShotSound(c);
        }
        buttonsReleased = false;
        isWaitingForCombo = false;
      }
    } else {
      if (isAnyBtnPressed && buttonsReleased && (now - lastFireTime > FIRE_COOLDOWN)) {
        int c = 0;
        if (b) c = 1;
        else if (r) c = 2;
        else if (g) c = 3;
        if (c > 0) {
          shots.push_back({ 0.0, c });
          stat_totalShots++;
          stat_lastGameShots++;  // STATS UPDATE
          lastFireTime = now;
          playShotSound(c);
        }
        buttonsReleased = false;
      }
    }

    // SHOT MOVEMENT
    float moveStep = (float)config_shot_speed_pct / 60.0;
    moveStep = moveStep * 0.6;
    if (moveStep < 0.2) moveStep = 0.2;

    for (int i = shots.size() - 1; i >= 0; i--) {
      shots[i].position += moveStep;
      bool remove = false;

      // HIT DETECTION
      if (currentState == STATE_PLAYING) {
        if (shots[i].position >= enemyFrontIndex && !enemies.empty()) {
          if (shots[i].color == enemies[0].color) {
            enemies.erase(enemies.begin());
            stat_totalKills++;  // STATS
            enemyFrontIndex += 1.0;
            flashPixel((int)shots[i].position);
            remove = true;
            checkWinCondition();
          } else {
            enemies.insert(enemies.begin(), { shots[i].color, 0.0 });
            enemyFrontIndex -= 1.0;
            remove = true;
            playSound(EVT_MISTAKE);
          }
        }
      } else if (currentState == STATE_BOSS_PLAYING) {
        // BOSS PROJECTILES COLLISION
        for (int p = 0; p < bossProjectiles.size(); p++) {
          if (shots[i].position >= bossProjectiles[p].pos) {
            if (shots[i].color == bossProjectiles[p].color) {
              bossProjectiles.erase(bossProjectiles.begin() + p);
              flashPixel((int)shots[i].position);
            }
            remove = true;
            break;
          }
        }

        // BOSS SEGMENT COLLISION
        if (!remove && shots[i].position >= enemyFrontIndex && !bossSegments.empty()) {
          int hitIndex = (int)(shots[i].position - enemyFrontIndex);
          if (hitIndex >= 0 && hitIndex < bossSegments.size()) {
            bool vulnerable = false;
            if (currentBossType == 1) vulnerable = true;
            else if (currentBossType == 2) {
              if (boss2State == B2_MOVE && bossSegments[hitIndex].active) vulnerable = true;
            } else if (currentBossType == 3) {
              if (boss3State != B3_PHASE_CHANGE) vulnerable = true;
            }

            if (vulnerable) {
              if (shots[i].color == bossSegments[hitIndex].color) {
                flashPixel((int)shots[i].position);
                bossSegments[hitIndex].hp--;
                if (bossSegments[hitIndex].hp <= 0) {
                  bossSegments.erase(bossSegments.begin() + hitIndex);
                  stat_totalKills++;  // STATS
                  if (hitIndex == 0) enemyFrontIndex += 1.0;
                  playSound(EVT_HIT_SUCCESS);
                }
                checkWinCondition();
              }
            }
            remove = true;
          }
        }
      }
      if (shots[i].position >= config_num_leds) remove = true;
      if (remove) shots.erase(shots.begin() + i);
    }

    // ENEMY MOVEMENT
    if (currentState == STATE_PLAYING) {
      float enemySpeed = (float)levels[currentLevel].speed;
      float eStep = enemySpeed / 60.0;
      enemyFrontIndex -= eStep;
      if (enemyFrontIndex <= config_homebase_size) { triggerBaseDestruction(); }
    } else if (currentState == STATE_BOSS_PLAYING) {
      int pSpeed = 60;
      if (currentBossType == 1) pSpeed = boss1Cfg.shotSpeed;
      if (currentBossType == 2) pSpeed = boss2Cfg.shotSpeed;
      moveBossProjectiles((float)pSpeed);

      if (currentBossType == 1) {
        float bStep = (float)boss1Cfg.moveSpeed / 60.0;
        enemyFrontIndex -= bStep;
        if (enemyFrontIndex <= config_homebase_size) { triggerBaseDestruction(); }
        if (now - bossActionTimer > (boss1Cfg.shotFreq * 100)) {
          bossActionTimer = now;
          int shotColor = 0;
          int frontColor = 0;
          if (bossSegments.size() > 0) frontColor = bossSegments[0].color;
          if (random(100) < 20 && frontColor > 0) {
            shotColor = frontColor;
          } else {
            do { shotColor = random(1, 4); } while (shotColor == frontColor && frontColor > 0);
          }
          bossProjectiles.push_back({ enemyFrontIndex, shotColor });
        }
      } else if (currentBossType == 2) {
        if (boss2State == B2_MOVE) {
          float bStep = (float)boss2Cfg.moveSpeed / 60.0;
          enemyFrontIndex -= bStep;
          if (boss2Section < 3) {
            if (enemyFrontIndex <= markerPos[boss2Section]) {
              boss2State = B2_CHARGE;
              bossActionTimer = now;
            }
          }
          if (enemyFrontIndex <= config_homebase_size) { triggerBaseDestruction(); }
        } else if (boss2State == B2_CHARGE) {
          if (now - bossActionTimer < (boss2Cfg.shotFreq * 100)) {
            if (now % 100 < 20) boss2LockedColor = random(1, 4);
          } else {
            boss2State = B2_SHOOT;
            boss2ShotsFired = 0;
            bossActionTimer = now;
            int startRange = 0;
            int endRange = 0;
            if (boss2Section == 0) {
              startRange = 0;
              endRange = 2;
            } else if (boss2Section == 1) {
              startRange = 0;
              endRange = 5;
            } else {
              startRange = 0;
              endRange = 8;
            }
            for (auto &seg : bossSegments) {
              if (seg.originalIndex >= startRange && seg.originalIndex <= endRange) seg.color = boss2LockedColor;
            }
          }
        } else if (boss2State == B2_SHOOT) {
          if (now - bossActionTimer > 150) {
            bossActionTimer = now;
            bossProjectiles.push_back({ enemyFrontIndex, boss2LockedColor });
            boss2ShotsFired++;
            if (boss2ShotsFired >= 10) {
              int startRange = 0;
              int endRange = 0;
              if (boss2Section == 0) {
                startRange = 0;
                endRange = 2;
              } else if (boss2Section == 1) {
                startRange = 3;
                endRange = 5;
              } else {
                startRange = 0;
                endRange = 8;
              }
              for (auto &seg : bossSegments) {
                if (seg.originalIndex >= startRange && seg.originalIndex <= endRange) seg.active = true;
              }
              boss2State = B2_MOVE;
              boss2Section++;
            }
          }
        }
      } else if (currentBossType == 3) {
        // --- LOGIC: SAFE FIRE ZONE ---
        // Wenn Strip > 180 LEDs (echtes Setup), nicht mehr schießen ab LED 70.
        // Wenn Strip <= 180 LEDs (Test/Notfall), schießen bis kurz vor die Basis (Basis + 5).
        float safeFireLimit = (config_num_leds > 180) ? 70.0 : (float)(config_homebase_size + 5);

        // --- PHASE CHANGE TRIGGER ---
        if (boss3State == B3_MOVE && boss3PhaseIndex < 2 && enemyFrontIndex <= boss3Markers[boss3PhaseIndex]) {
          boss3State = B3_PHASE_CHANGE;
          bossActionTimer = now;
        }

        // --- STATE HANDLING ---
        if (boss3State == B3_MOVE) {
          float bStep = (float)boss3Cfg.moveSpeed / 60.0;
          enemyFrontIndex -= bStep;
          if (enemyFrontIndex <= config_homebase_size) { triggerBaseDestruction(); }

          // CHECK: Nur schießen, wenn wir noch vor der Safe-Zone sind
          if (enemyFrontIndex > safeFireLimit && boss3Cfg.shotFreq > 0 && (now - bossActionTimer > (boss3Cfg.shotFreq * 100))) {
            bossActionTimer = now;
            bossProjectiles.push_back({ enemyFrontIndex, (int)random(1, 4) });
          }
        } else if (boss3State == B3_PHASE_CHANGE) {
          if (now - bossActionTimer > 4000) {
            boss3State = B3_BURST;
            boss3BurstCounter = 0;
            bossActionTimer = now;
            for (auto &seg : bossSegments) seg.color = random(4, 8);
            boss3PhaseIndex++;
          }
        } else if (boss3State == B3_BURST) {
          if (now - bossActionTimer > 200) {
            bossActionTimer = now;
            // CHECK: Auch Burst-Attacken respektieren die Safe-Zone
            if (enemyFrontIndex > safeFireLimit) {
              bossProjectiles.push_back({ enemyFrontIndex, (int)random(1, 8) });
            }
            boss3BurstCounter++;
            if (boss3BurstCounter >= boss3Cfg.burstCount) {
              boss3State = B3_WAIT;
              bossActionTimer = now;
            }
          }
        } else if (boss3State == B3_WAIT) {
          if (now - bossActionTimer > 2000) {
            boss3State = B3_MOVE;
            bossActionTimer = now;
          }
        }
      }
    }

    FastLED.clear();
    if (currentState == STATE_BOSS_PLAYING) {
      if (currentBossType == 2) {
        for (int i = 0; i < 3; i++) {
          if (markerPos[i] < enemyFrontIndex) leds[markerPos[i] + ledStartOffset] = CRGB(50, 0, 0);
        }
      } else if (currentBossType == 3) {
        if (boss3PhaseIndex <= 0) {
          leds[boss3Markers[0] + ledStartOffset] = CRGB(50, 0, 0);
          leds[boss3Markers[0] + ledStartOffset + 1] = CRGB(50, 0, 0);
        }
        if (boss3PhaseIndex <= 1) {
          leds[boss3Markers[1] + ledStartOffset] = CRGB(50, 0, 0);
          leds[boss3Markers[1] + ledStartOffset + 1] = CRGB(50, 0, 0);
        }
      }
    }
    if (currentState == STATE_PLAYING) {
      for (int i = 0; i < enemies.size(); i++) {
        float pos = enemyFrontIndex + (float)i;
        drawCrispPixel(pos, getColor(enemies[i].color));
      }
    } else if (currentState == STATE_BOSS_PLAYING) {
      for (int i = 0; i < bossSegments.size(); i++) {
        float pos = enemyFrontIndex + (float)i;
        if (pos < config_num_leds && pos >= 0) {
          CRGB c = getColor(bossSegments[i].color);
          if (currentBossType == 2) {
            c = col_cb;
            if (boss2State == B2_MOVE) {
              if (bossSegments[i].active) {
                c = getColor(bossSegments[i].color);
                if ((millis() / 100) % 2 == 0) c = CRGB::Black;
              }
            } else if (boss2State == B2_CHARGE || boss2State == B2_SHOOT) {
              int oid = bossSegments[i].originalIndex;
              bool highlight = false;
              if (boss2Section == 0) {
                if (oid >= 0 && oid <= 2) highlight = true;
              } else if (boss2Section == 1) {
                if (oid >= 0 && oid <= 5) highlight = true;
              } else if (boss2Section >= 2) {
                highlight = true;
              }
              if (highlight) c = getColor(boss2LockedColor);
            }
          } else if (currentBossType == 3 && boss3State == B3_PHASE_CHANGE) {
            c = CRGB::White;
          }
          drawCrispPixel(pos, c);
        }
      }
      for (auto &p : bossProjectiles) {
        drawCrispPixel(p.pos, getColor(p.color));
      }
    }
    for (auto &s : shots) {
      drawCrispPixel(s.position, getColor(s.color));
    }
    for (int i = 0; i < config_homebase_size; i++) leds[i + ledStartOffset] = CRGB::White;
    if (config_sacrifice_led) leds[0] = CRGB(20, 0, 0);
    FastLED.show();
  }
}
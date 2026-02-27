// --------------------------------------------------------------------------
// 7. CONFIG & WEB HANDLERS (MUST BE BEFORE SETUP)
// --------------------------------------------------------------------------
void loadColors() {
  preferences.begin("colors", true);
  hex_c1 = preferences.getString("c1", "#0000FF");
  hex_c2 = preferences.getString("c2", "#FF0000");
  hex_c3 = preferences.getString("c3", "#00FF00");
  hex_c4 = preferences.getString("c4", "#FFFF00");
  hex_c5 = preferences.getString("c5", "#FF00FF");
  hex_c6 = preferences.getString("c6", "#00FFFF");
  hex_cw = preferences.getString("cw", "#FFFFFF");
  hex_cb = preferences.getString("cb", "#222222");
  preferences.end();

  col_c1 = hexToCRGB(hex_c1);
  col_c2 = hexToCRGB(hex_c2);
  col_c3 = hexToCRGB(hex_c3);
  col_c4 = hexToCRGB(hex_c4);
  col_c5 = hexToCRGB(hex_c5);
  col_c6 = hexToCRGB(hex_c6);
  col_cw = hexToCRGB(hex_cw);
  col_cb = hexToCRGB(hex_cb);
}

void handleSaveColors() {
  if (server.hasArg("c1")) hex_c1 = server.arg("c1");
  if (server.hasArg("c2")) hex_c2 = server.arg("c2");
  if (server.hasArg("c3")) hex_c3 = server.arg("c3");
  if (server.hasArg("c4")) hex_c4 = server.arg("c4");
  if (server.hasArg("c5")) hex_c5 = server.arg("c5");
  if (server.hasArg("c6")) hex_c6 = server.arg("c6");
  if (server.hasArg("cw")) hex_cw = server.arg("cw");
  if (server.hasArg("cb")) hex_cb = server.arg("cb");

  preferences.begin("colors", false);
  preferences.putString("c1", hex_c1);
  preferences.putString("c2", hex_c2);
  preferences.putString("c3", hex_c3);
  preferences.putString("c4", hex_c4);
  preferences.putString("c5", hex_c5);
  preferences.putString("c6", hex_c6);
  preferences.putString("cw", hex_cw);
  preferences.putString("cb", hex_cb);
  preferences.end();

  loadColors();
  server.sendHeader("Location", "/colors");
  server.send(303);
}

void loadSounds() {
  preferences.begin("snds", true);
  cfg_snd_start = preferences.getString("s_start", DEF_SND_START);
  cfg_snd_win = preferences.getString("s_win", DEF_SND_WIN);
  cfg_snd_lose = preferences.getString("s_lose", DEF_SND_LOSE);
  cfg_snd_mistake = preferences.getString("s_mistake", DEF_SND_MISTAKE);
  cfg_snd_hit = preferences.getString("s_hit", DEF_SND_HIT);
  cfg_snd_shot_b = preferences.getString("s_shot_b", DEF_SND_SHOT_BLUE);
  cfg_snd_shot_r = preferences.getString("s_shot_r", DEF_SND_SHOT_RED);
  cfg_snd_shot_g = preferences.getString("s_shot_g", DEF_SND_SHOT_GREEN);
  cfg_snd_shot_w = preferences.getString("s_shot_w", DEF_SND_SHOT_WHITE);
  preferences.end();

  melodyFromStr(melStart, cfg_snd_start);
  melodyFromStr(melWin, cfg_snd_win);
  melodyFromStr(melLose, cfg_snd_lose);
  melodyFromStr(melMistake, cfg_snd_mistake);
  melodyFromStr(melHit, cfg_snd_hit);
  melodyFromStr(melShotBlue, cfg_snd_shot_b);
  melodyFromStr(melShotRed, cfg_snd_shot_r);
  melodyFromStr(melShotGreen, cfg_snd_shot_g);
  melodyFromStr(melShotWhite, cfg_snd_shot_w);
}

void handleSaveSounds() {
  cfg_snd_start = server.arg("s_start");
  cfg_snd_win = server.arg("s_win");
  cfg_snd_lose = server.arg("s_lose");
  cfg_snd_mistake = server.arg("s_mistake");
  cfg_snd_hit = server.arg("s_hit");
  cfg_snd_shot_b = server.arg("s_shot_b");
  cfg_snd_shot_r = server.arg("s_shot_r");
  cfg_snd_shot_g = server.arg("s_shot_g");
  cfg_snd_shot_w = server.arg("s_shot_w");

  preferences.begin("snds", false);
  preferences.putString("s_start", cfg_snd_start);
  preferences.putString("s_win", cfg_snd_win);
  preferences.putString("s_lose", cfg_snd_lose);
  preferences.putString("s_mistake", cfg_snd_mistake);
  preferences.putString("s_hit", cfg_snd_hit);
  preferences.putString("s_shot_b", cfg_snd_shot_b);
  preferences.putString("s_shot_r", cfg_snd_shot_r);
  preferences.putString("s_shot_g", cfg_snd_shot_g);
  preferences.putString("s_shot_w", cfg_snd_shot_w);
  preferences.end();

  loadSounds();
  server.sendHeader("Location", "/sounds");
  server.send(303);
}

void applyProfileDefaults(String prefix) {
  if (prefix == "def_") {
    // STANDARD PROFILE
    levels[1] = { 5, 15, 0 };
    levels[2] = { 6, 20, 0 };
    levels[3] = { 7, 25, 2 };  // Masterblaster
    levels[4] = { 8, 30, 0 };
    levels[5] = { 9, 35, 0 };
    levels[6] = { 10, 40, 1 };  // The Tank
    levels[7] = { 20, 20, 0 };
    levels[8] = { 20, 25, 0 };
    levels[9] = { 10, 60, 0 };
    levels[10] = { 14, 60, 3 };
    boss1Cfg = { 4, 60, 4, 30, 0, 0, 0, 0 };
    boss2Cfg = { 10, 60, 5, 40, 0, 85, 55, 30 };
    boss3Cfg = { 7, 50, 3, 60, 5, 0, 0, 0 };
  } else if (prefix == "kid_") {
    // KIDS PROFILE (EASY)
    levels[1] = { 5, 15, 0 };
    levels[2] = { 5, 20, 0 };
    levels[3] = { 6, 25, 2 };
    levels[4] = { 6, 20, 0 };
    levels[5] = { 7, 25, 0 };
    levels[6] = { 10, 40, 1 };
    levels[7] = { 8, 30, 0 };
    levels[8] = { 8, 35, 0 };
    levels[9] = { 10, 20, 0 };
    levels[10] = { 14, 60, 3 };
    boss1Cfg = { 4, 60, 2, 40, 0, 0, 0, 0 };
    boss2Cfg = { 7, 40, 3, 40, 0, 85, 55, 30 };
    boss3Cfg = { 4, 40, 1, 80, 1, 0, 0, 0 };
  } else {
    // PRO PROFILE
    for (int i = 1; i <= 10; i++) { levels[i] = { 5 + i, 15 + (i * 5), 0 }; }
    levels[3].bossType = 2;
    levels[6].bossType = 1;
    levels[10].bossType = 3;
    boss1Cfg = { 6, 80, 5, 25, 0, 0, 0, 0 };
    boss2Cfg = { 15, 80, 6, 30, 0, 90, 60, 30 };
    boss3Cfg = { 15, 60, 6, 40, 8, 0, 0, 0 };
  }
}

void saveCurrentToPreferences(String prefix) {
  preferences.begin("game", false);
  preferences.putInt((prefix + "leds").c_str(), config_num_leds);
  preferences.putInt((prefix + "bright").c_str(), config_brightness_pct);
  preferences.putInt((prefix + "startlvl").c_str(), config_start_level);

  for (int i = 1; i <= 10; i++) {
    preferences.putInt((prefix + "l" + String(i) + "s").c_str(), levels[i].speed);
    preferences.putInt((prefix + "l" + String(i) + "l").c_str(), levels[i].length);
    preferences.putInt((prefix + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }
  preferences.putBytes((prefix + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  preferences.putBytes((prefix + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  preferences.putBytes((prefix + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));
  preferences.end();
}

void performFactoryReset() {
  preferences.begin("game", true);
  String s = preferences.getString("ssid", "");
  String p = preferences.getString("pass", "");
  String ip = preferences.getString("sip_ip", "");
  String gw = preferences.getString("sip_gw", "");
  String sn = preferences.getString("sip_sn", "");
  String dns = preferences.getString("sip_dns", "");
  bool sip = preferences.getBool("sip_on", false);
  preferences.end();

  preferences.begin("game", false);
  preferences.clear();
  preferences.putString("ssid", s);
  preferences.putString("pass", p);
  preferences.putBool("sip_on", sip);
  preferences.putString("sip_ip", ip);
  preferences.putString("sip_gw", gw);
  preferences.putString("sip_sn", sn);
  preferences.putString("sip_dns", dns);

  preferences.putBool("sac_led", true);
  preferences.putInt("hb_size", 3);
  preferences.putInt("shot_spd", 100);

  preferences.putBool("snd_on", true);
  preferences.putInt("snd_vol", 50);

  // STATS RESET? Typically factory reset wipes stats too.
  preferences.putULong("st_shots", 0);
  preferences.putULong("st_kills", 0);

  preferences.putInt("version", CONFIG_VERSION);
  preferences.putString("act_prof", "def_");
  preferences.end();

  preferences.begin("snds", false);
  preferences.clear();
  preferences.end();
  preferences.begin("colors", false);
  preferences.clear();
  preferences.end();

  applyProfileDefaults("def_");
  saveCurrentToPreferences("def_");
  applyProfileDefaults("kid_");
  saveCurrentToPreferences("kid_");
  applyProfileDefaults("pro_");
  saveCurrentToPreferences("pro_");
  applyProfileDefaults("def_");
}

void setupDefaultConfig() {
  applyProfileDefaults("def_");
}

void loadConfig(String prefix) {
  preferences.begin("game", true);
  config_num_leds = preferences.getInt((prefix + "leds").c_str(), config_num_leds);
  config_brightness_pct = preferences.getInt((prefix + "bright").c_str(), config_brightness_pct);
  config_start_level = preferences.getInt((prefix + "startlvl").c_str(), config_start_level);
  config_ssid = preferences.getString("ssid", "");
  config_pass = preferences.getString("pass", "");
  config_static_ip = preferences.getBool("sip_on", false);
  config_ip = preferences.getString("sip_ip", "");
  config_gateway = preferences.getString("sip_gw", "");
  config_subnet = preferences.getString("sip_sn", "");
  config_dns = preferences.getString("sip_dns", "");

  config_sacrifice_led = preferences.getBool("sac_led", true);
  config_homebase_size = preferences.getInt("hb_size", 3);
  config_shot_speed_pct = preferences.getInt("shot_spd", 100);
  ledStartOffset = config_sacrifice_led ? 1 : 0;

  config_sound_on = preferences.getBool("snd_on", true);
  config_volume_pct = preferences.getInt("snd_vol", 50);

  for (int i = 1; i <= 10; i++) {
    levels[i].speed = preferences.getInt((prefix + "l" + String(i) + "s").c_str(), levels[i].speed);
    levels[i].length = preferences.getInt((prefix + "l" + String(i) + "l").c_str(), levels[i].length);
    levels[i].bossType = preferences.getInt((prefix + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }

  if (preferences.isKey((prefix + "b1").c_str())) preferences.getBytes((prefix + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  if (preferences.isKey((prefix + "b2").c_str())) preferences.getBytes((prefix + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  if (preferences.isKey((prefix + "b3").c_str())) preferences.getBytes((prefix + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));
  preferences.end();
}

void handleProfileSwitch() {
  if (server.hasArg("profile")) {
    String p = server.arg("profile");
    if (p == "kid") currentProfilePrefix = "kid_";
    else if (p == "pro") currentProfilePrefix = "pro_";
    else currentProfilePrefix = "def_";
    preferences.begin("game", false);
    preferences.putString("act_prof", currentProfilePrefix);
    preferences.end();
    applyProfileDefaults(currentProfilePrefix);
    loadConfig(currentProfilePrefix);
    loadHighscores();
    server.sendHeader("Location", "/");
    server.send(303);
  } else server.send(400, "text/plain", "Bad Request");
}

void handleReset() {
  performFactoryReset();
  server.send(200, "text/html", "<h2>Reset successful!</h2><p>Values & Scores wiped. ESP restarting.</p>");
  delay(1000);
  ESP.restart();
}

void handleSave() {
  if (server.hasArg("leds")) config_num_leds = server.arg("leds").toInt();
  if (server.hasArg("bright")) config_brightness_pct = server.arg("bright").toInt();
  if (server.hasArg("startlvl")) config_start_level = server.arg("startlvl").toInt();
  if (server.hasArg("ssid")) config_ssid = server.arg("ssid");
  if (server.hasArg("pass")) config_pass = server.arg("pass");
  config_static_ip = server.hasArg("static_ip");
  config_ip = server.arg("ip");
  config_gateway = server.arg("gw");
  config_subnet = server.arg("sn");
  config_dns = server.arg("dns");
  if (server.hasArg("hb_size")) config_homebase_size = server.arg("hb_size").toInt();
  if (server.hasArg("shot_spd")) config_shot_speed_pct = server.arg("shot_spd").toInt();
  config_sacrifice_led = server.hasArg("sac_led");

  config_sound_on = server.hasArg("snd_on");
  if (server.hasArg("vol")) config_volume_pct = server.arg("vol").toInt();

  preferences.begin("game", false);
  preferences.putString("ssid", config_ssid);
  preferences.putString("pass", config_pass);
  preferences.putBool("sip_on", config_static_ip);
  preferences.putString("sip_ip", config_ip);
  preferences.putString("sip_gw", config_gateway);
  preferences.putString("sip_sn", config_subnet);
  preferences.putString("sip_dns", config_dns);
  preferences.putBool("sac_led", config_sacrifice_led);
  preferences.putInt("hb_size", config_homebase_size);
  preferences.putInt("shot_spd", config_shot_speed_pct);

  preferences.putBool("snd_on", config_sound_on);
  preferences.putInt("snd_vol", config_volume_pct);

  String p = currentProfilePrefix;
  preferences.putInt((p + "leds").c_str(), config_num_leds);
  preferences.putInt((p + "bright").c_str(), config_brightness_pct);
  preferences.putInt((p + "startlvl").c_str(), config_start_level);
  for (int i = 1; i <= 10; i++) {
    levels[i].speed = server.arg("lspd" + String(i)).toInt();
    levels[i].length = server.arg("llen" + String(i)).toInt();
    levels[i].bossType = server.arg("lboss" + String(i)).toInt();
    preferences.putInt((p + "l" + String(i) + "s").c_str(), levels[i].speed);
    preferences.putInt((p + "l" + String(i) + "l").c_str(), levels[i].length);
    preferences.putInt((p + "l" + String(i) + "b").c_str(), levels[i].bossType);
  }
  boss1Cfg.moveSpeed = server.arg("b1mv").toInt();
  boss1Cfg.shotSpeed = server.arg("b1ss").toInt();
  boss1Cfg.hpPerLed = server.arg("b1hp").toInt();
  boss1Cfg.shotFreq = server.arg("b1fr").toInt();
  preferences.putBytes((p + "b1").c_str(), &boss1Cfg, sizeof(BossConfig));
  boss2Cfg.moveSpeed = server.arg("b2mv").toInt();
  boss2Cfg.shotSpeed = server.arg("b2ss").toInt();
  boss2Cfg.hpPerLed = server.arg("b2hp").toInt();
  boss2Cfg.shotFreq = server.arg("b2fr").toInt();
  boss2Cfg.m1 = server.arg("b2m1").toInt();
  boss2Cfg.m2 = server.arg("b2m2").toInt();
  boss2Cfg.m3 = server.arg("b2m3").toInt();
  preferences.putBytes((p + "b2").c_str(), &boss2Cfg, sizeof(BossConfig));
  boss3Cfg.moveSpeed = server.arg("b3mv").toInt();
  boss3Cfg.shotSpeed = 0;
  boss3Cfg.hpPerLed = server.arg("b3hp").toInt();
  boss3Cfg.shotFreq = server.arg("b3fr").toInt();
  boss3Cfg.burstCount = server.arg("b3bc").toInt();
  preferences.putBytes((p + "b3").c_str(), &boss3Cfg, sizeof(BossConfig));
  preferences.end();
  server.send(200, "text/html", "<h2>Saved!</h2><p>ESP restarting...</p><a href='/'>Go Back</a>");
  delay(1000);
  ESP.restart();
}

String getUpdateHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Firmware Update</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px;max-width:600px;margin:auto;} h2{color:#0f0;} input{width:100%;margin-bottom:10px;padding:10px;}</style>";
  h += "</head><body><h2>SYSTEM UPDATE</h2>";
  h += "<p>Upload .bin file from Arduino IDE (Sketch -> Export Compiled Binary).</p>";
  h += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><br><button type='submit' style='padding:15px;background:#00f;color:white;font-weight:bold;width:100%;cursor:pointer;'>UPDATE FIRMWARE</button></form>";
  h += "<br><a href='/' style='color:#0f0;'>Back</a></body></html>";
  return h;
}

String getColorHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Color Config</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;} .sec{margin-top:15px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;} h3{margin-top:0;color:#0f0;} input[type=color] { height: 50px; cursor: pointer; } a { color: #00ff00; text-decoration: none; }</style>";
  h += "</head><body>";
  h += "<h2>🎨 CUSTOM COLORS</h2>";
  h += "<form action='/savecolors' method='POST'>";

  h += "<div class='sec'><h3>Main Enemies (Types 1-3)</h3>";
  h += "Type 1 (Blue btn): <input type='color' name='c1' value='" + hex_c1 + "'>";
  h += "Type 2 (Red btn): <input type='color' name='c2' value='" + hex_c2 + "'>";
  h += "Type 3 (Green btn): <input type='color' name='c3' value='" + hex_c3 + "'></div>";

  h += "<div class='sec'><h3>Boss / Mix Colors</h3>";
  h += "Mix Color 1: <input type='color' name='c4' value='" + hex_c4 + "'>";
  h += "Mix Color 2: <input type='color' name='c5' value='" + hex_c5 + "'>";
  h += "Mix Color 3: <input type='color' name='c6' value='" + hex_c6 + "'>";
  h += "Boss Generic/Dark: <input type='color' name='cb' value='" + hex_cb + "'></div>";

  h += "<div class='sec'><h3>Player Shots</h3>";
  h += "Combo Shot (White): <input type='color' name='cw' value='" + hex_cw + "'></div>";

  h += "<br><button type='submit' style='background:#009900;font-size:1.2em;'>SAVE COLORS</button>";
  h += "</form><br><a href='/'>&laquo; Back to Main Menu</a>";
  h += "</body></html>";
  return h;
}

String getSoundHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Sound Config</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;} .sec{margin-top:15px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;} h3{margin-top:0;color:#0f0;} .info{font-size:0.9em;color:#aaa;background:#333;padding:10px;border-radius:4px;margin-bottom:15px;} a { color: #00ff00; text-decoration: none; }</style>";
  h += "</head><body>";
  h += "<h2>🎵 CUSTOM SOUND EDITOR</h2>";
  h += "<div class='info'><b>How to use:</b><br>Format: <code>Freq,Duration;Freq,Duration</code><br>Example: <code>523,100;659,100</code> (Plays C5 then E5)<br>Leave empty for silence.<br><br><b>Ask Gemini/AI:</b><br><i>\"Create a sound string for [Effect] using pairs of Frequency(Hz),Duration(ms) separated by semicolons. E.g. a rising laser sound.\"</i></div>";
  h += "<form action='/savesounds' method='POST'>";
  h += "<div class='sec'><h3>Game Events</h3>";
  h += "Start: <input name='s_start' value='" + cfg_snd_start + "'>";
  h += "Win Level: <input name='s_win' value='" + cfg_snd_win + "'>";
  h += "Game Over: <input name='s_lose' value='" + cfg_snd_lose + "'>";
  h += "Mistake/Grow: <input name='s_mistake' value='" + cfg_snd_mistake + "'>";
  h += "Hit Success: <input name='s_hit' value='" + cfg_snd_hit + "'></div>";
  h += "<div class='sec'><h3>Player Shots</h3>";
  h += "Blue Shot: <input name='s_shot_b' value='" + cfg_snd_shot_b + "'>";
  h += "Red Shot: <input name='s_shot_r' value='" + cfg_snd_shot_r + "'>";
  h += "Green Shot: <input name='s_shot_g' value='" + cfg_snd_shot_g + "'>";
  h += "White Shot: <input name='s_shot_w' value='" + cfg_snd_shot_w + "'></div>";
  h += "<br><button type='submit' style='background:#009900;font-size:1.2em;'>SAVE SOUNDS</button>";
  h += "</form><br><a href='/'>&laquo; Back to Main Menu</a>";
  h += "</body></html>";
  return h;
}

String getHTML() {
  String h = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  h += "<title>Ultimate RGB Invaders</title>";
  h += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:10px;max-width:800px;margin:auto;}input,select,button{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;box-sizing:border-box;margin-bottom:5px;}table{width:100%;border-collapse:collapse;margin-bottom:10px;} td,th{border:1px solid #444;padding:6px;text-align:center;}.sec{margin-top:25px;border-top:1px solid #555;padding-top:15px;background:#222;padding:15px;border-radius:5px;}#warning-box{margin-top:10px;padding:10px;background:#b30000;color:#fff;border:1px solid #ff0000;display:none;font-weight:bold;border-radius:4px;}.val-highlight{color:#0f0;font-weight:bold;}h2,h3{margin-top:0;} pre{font-family:monospace;color:#0f0;font-size:12px;overflow-x:auto;} .score-box{background:#004d00; border:2px solid #00ff00; padding:15px; text-align:center; margin-bottom:15px; border-radius:8px;} .big-score{font-size:32px; font-weight:bold; color:#fff;} .small-score{font-size:14px; color:#aaa;}";
  h += " .b-card { background: #2a2a2a; padding: 15px; border-radius: 5px; margin-bottom: 15px; border: 1px solid #444; }";
  h += " .b-head { font-size: 1.1em; font-weight: bold; margin-bottom: 10px; color: #fff; border-bottom: 1px solid #555; padding-bottom: 5px; }";
  h += " .form-grid { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 10px; }";
  h += " .f-item { display: flex; flex-direction: column; }";
  h += " .lbl { font-size: 11px; color: #aaa; margin-bottom: 4px; }";
  h += " .neon-text { font-family: sans-serif; font-weight: 900; font-size: 3.8em; text-align: center; text-transform: uppercase; letter-spacing: 4px; margin: 20px 0; background: linear-gradient(90deg, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000); background-size: 400%; -webkit-background-clip: text; background-clip: text; color: rgba(255,255,255,0.1); animation: rgbAnim 4s linear infinite; text-shadow: 0 0 20px rgba(255,255,255,0.2); }";
  h += " @keyframes rgbAnim { 0% { background-position: 0%; } 100% { background-position: 400%; } }";
  h += " .sub-head { text-align: center; font-size: 0.8em; color: #888; margin-bottom: 20px; }";
  h += " .credits { text-align: center; margin-top: 30px; font-size: 0.8em; color: #555; border-top: 1px solid #333; padding-top: 10px; }";
  h += " a { color: #00ff00; text-decoration: none; }";
  h += " .stat-grid { display:flex; gap:10px; text-align:center; } .stat-box { flex:1; background:#222; padding:10px; border-radius:5px; border:1px solid #444; } .stat-val { font-size:1.5em; font-weight:bold; color:#0ff; }";
  h += "</style>";
  h += "<script>function updateCalc() { var count = parseInt(document.getElementById('ledCount').value)||0; var brightPct = parseInt(document.getElementById('brightness').value)||50; document.getElementById('brightVal').innerText = brightPct + '%'; var warn = document.getElementById('warning-box'); if (count > 0) { var brightFactor = brightPct / 100.0; var amps = ((count * 15 * brightFactor) + 120) / 1000; document.getElementById('ampValue').innerText = amps.toFixed(2) + ' A'; if(amps > 5.0) { warn.style.display='block'; warn.innerText='WARNING: > 5A! High Power PSU required!'; } else warn.style.display='none'; } } function toggleIP() { var x = document.getElementById('ipsettings'); if(document.getElementById('chkStatic').checked) x.style.display='block'; else x.style.display='none'; } function confirmReset() { return confirm('Really delete all settings and factory reset?'); } window.onload = function(){ updateCalc(); toggleIP(); };</script>";
  h += "</head><body>";
  h += "<div class='neon-text'>RGB INVADERS</div>";
  h += "<div class='sub-head'>created by Qwer.Tzui / WorksAsDesigned - Version 10.0 (Final)</div>";
  h += "<div class='score-box'>ALL TIME BEST<div class='big-score'>" + String(highScore) + "</div>";
  h += "<div class='small-score'>Last Games: " + String(lastGames[0]) + " | " + String(lastGames[1]) + " | " + String(lastGames[2]) + "</div></div>";

  // STATS DISPLAY
  h += "<div class='sec'><h3>Battle Statistics</h3><div class='stat-grid'>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_totalShots) + "</div><div>Total Shots</div></div>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_totalKills) + "</div><div>Alien Kills</div></div>";
  h += "<div class='stat-box'><div class='stat-val'>" + String(stat_lastGameShots) + "</div><div>Last Game Shots</div></div>";
  h += "</div></div>";

  // BUTTONS TO CONFIG PAGES
  h += "<div style='display:flex;gap:10px;justify-content:center;margin-bottom:20px;margin-top:20px;'>";
  h += "<a href='/sounds' style='flex:1;'><button style='background:#ff00ff;font-weight:bold;font-size:1.1em;padding:12px;'>🎵 SOUNDS</button></a>";
  h += "<a href='/colors' style='flex:1;'><button style='background:#00ffff;color:#000;font-weight:bold;font-size:1.1em;padding:12px;'>🎨 COLOR CONFIG</button></a>";
  h += "</div>";

  String pName = (currentProfilePrefix == "def_") ? "Standard" : ((currentProfilePrefix == "kid_") ? "Kids" : "Pro");
  h += "<div class='sec'><h3>Profile Management</h3>Current Profile: <b>" + pName + "</b><br><form action='/loadprofile' method='POST' style='display:flex;gap:5px;margin-top:5px;'><select name='profile'><option value='def' " + String(currentProfilePrefix == "def_" ? "selected" : "") + ">Standard</option><option value='kid' " + String(currentProfilePrefix == "kid_" ? "selected" : "") + ">Kids</option><option value='pro' " + String(currentProfilePrefix == "pro_" ? "selected" : "") + ">Pro/Party</option></select><button type='submit'>Load Profile</button></form></div>";

  h += "<form action='/save' method='POST'><div class='sec'><h3>Hardware & General</h3>Start Level: <input type='number' name='startlvl' min='1' max='10' value='" + String(config_start_level) + "'><br>";
  h += "Total LEDs: <input id='ledCount' type='number' name='leds' value='" + String(config_num_leds) + "' oninput='updateCalc()'><br>";
  h += "<label>Sacrificial LED: <input type='checkbox' name='sac_led' value='1' " + String(config_sacrifice_led ? "checked" : "") + " style='width:auto;'></label><br>";
  h += "<label>Homebase Size: <input type='number' name='hb_size' min='1' max='5' value='" + String(config_homebase_size) + "'></label><br>";
  h += "<label>Player Shot Speed: <span id='shotVal'>" + String(config_shot_speed_pct) + "%</span></label><input type='range' name='shot_spd' min='50' max='150' value='" + String(config_shot_speed_pct) + "' oninput=\"document.getElementById('shotVal').innerText = this.value + '%';\"><br>";
  h += "<label>Default Brightness: <span id='brightVal'>" + String(config_brightness_pct) + "%</span></label><input id='brightness' type='range' name='bright' min='10' max='100' value='" + String(config_brightness_pct) + "' oninput='updateCalc()'><div style='margin-top:5px;'>Est. Current: <span id='ampValue' class='val-highlight'>0.00 A</span></div>";

  h += "<div style='margin-top:10px;border-top:1px dashed #555;padding-top:10px;'>";
  h += "<label>Sound Enabled: <input type='checkbox' name='snd_on' value='1' " + String(config_sound_on ? "checked" : "") + " style='width:auto;'></label><br>";
  h += "<label>Master Volume: <span id='volVal'>" + String(config_volume_pct) + "%</span></label><input type='range' name='vol' min='0' max='100' value='" + String(config_volume_pct) + "' oninput=\"document.getElementById('volVal').innerText = this.value + '%';\">";
  h += "</div>";

  h += "<div id='warning-box'></div></div>";
  h += "<div class='sec'><h3>Network</h3><label>WiFi SSID:</label><input type='text' name='ssid' value='" + config_ssid + "' placeholder='WiFi Name'>Password: <input type='password' name='pass' value='" + config_pass + "'><br><input type='checkbox' id='chkStatic' name='static_ip' value='1' onchange='toggleIP()' " + String(config_static_ip ? "checked" : "") + " style='width:auto;'> Static IP<br><div id='ipsettings' style='display:none;margin-top:10px;'>IP: <input name='ip' value='" + config_ip + "'>Gateway: <input name='gw' value='" + config_gateway + "'>Subnet: <input name='sn' value='" + config_subnet + "'>DNS: <input name='dns' value='" + config_dns + "'></div></div>";
  h += "<div class='sec'><h3>Level Configuration</h3><table><thead><tr><th>Lvl</th><th>Speed</th><th>Len</th><th>Boss?</th></tr></thead><tbody>";
  for (int i = 1; i <= 10; i++) { h += "<tr><td data-label='Level'>" + String(i) + "</td><td data-label='Speed'><input name='lspd" + String(i) + "' value='" + String(levels[i].speed) + "'></td><td data-label='Len/Type'><input name='llen" + String(i) + "' value='" + String(levels[i].length) + "'></td><td data-label='Boss'><select name='lboss" + String(i) + "'><option value='0' " + String(levels[i].bossType == 0 ? "selected" : "") + ">-</option><option value='2' " + String(levels[i].bossType == 2 ? "selected" : "") + ">Masterblaster</option><option value='1' " + String(levels[i].bossType == 1 ? "selected" : "") + ">The Tank</option><option value='3' " + String(levels[i].bossType == 3 ? "selected" : "") + ">RGB Overlord</option></select></td></tr>"; }
  h += "</tbody></table></div>";

  h += "<div class='sec'><h3>Boss Configuration</h3>";
  h += "<div class='b-card'><div class='b-head'>Masterblaster (Boss 1)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b2mv' value='" + String(boss2Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Speed</span><input name='b2ss' value='" + String(boss2Cfg.shotSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b2hp' value='" + String(boss2Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Reload (0.1s)</span><input name='b2fr' value='" + String(boss2Cfg.shotFreq) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 1 (%)</span><input name='b2m1' value='" + String(boss2Cfg.m1) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 2 (%)</span><input name='b2m2' value='" + String(boss2Cfg.m2) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Mark 3 (%)</span><input name='b2m3' value='" + String(boss2Cfg.m3) + "'></div>";
  h += "</div></div>";

  h += "<div class='b-card'><div class='b-head'>The Tank (Boss 2)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b1mv' value='" + String(boss1Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Speed</span><input name='b1ss' value='" + String(boss1Cfg.shotSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b1hp' value='" + String(boss1Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Freq</span><input name='b1fr' value='" + String(boss1Cfg.shotFreq) + "'></div>";
  h += "</div></div>";

  h += "<div class='b-card'><div class='b-head'>RGB Overlord (Boss 3)</div><div class='form-grid'>";
  h += "<div class='f-item'><span class='lbl'>Move Speed</span><input name='b3mv' value='" + String(boss3Cfg.moveSpeed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>HP/LED</span><input name='b3hp' value='" + String(boss3Cfg.hpPerLed) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Shot Freq</span><input name='b3fr' value='" + String(boss3Cfg.shotFreq) + "'></div>";
  h += "<div class='f-item'><span class='lbl'>Burst Count</span><input name='b3bc' value='" + String(boss3Cfg.burstCount) + "'></div>";
  h += "</div></div>";

  h += "</div>";

  h += "<br><input type='submit' value='SAVE SETTINGS' style='width:100%;background:#009900;padding:15px;font-size:1.2em;cursor:pointer;font-weight:bold;'></form>";
  h += "<div style='margin-top:20px;text-align:center;'><a href='/update'><button style='background:#0044cc;padding:10px;font-size:1em;'>⬇ FIRMWARE UPDATE ⬇</button></a></div>";
  h += "<br><br><form action='/reset' method='POST' onsubmit='return confirmReset()'><button type='submit' style='background:#990000;padding:10px;'>FACTORY RESET (Clear Scores)</button></form>";
  h += "<div class='credits'><a href='https://paypal.me/WeisWernau' target='_blank'>paypal.me/WeisWernau</a><br><br><i>\"I don't need your money. But if I can buy my wife a bouquet of flowers, the chance increases that I can publish more funny projects - every married man knows what I'm talking about.\"</i></div>";
  h += "</body></html>";
  return h;
}

void enableWiFi() {
  FastLED.clear();
  FastLED.show();
  WiFi.mode(WIFI_AP_STA);
  if (config_ssid != "") { WiFi.begin(config_ssid.c_str(), config_pass.c_str()); }
  if (config_static_ip && config_ip.length() > 0) {
    IPAddress ip, gw, sn, dns;
    if (ip.fromString(config_ip) && gw.fromString(config_gateway) && sn.fromString(config_subnet)) {
      if (config_dns.length() > 0) dns.fromString(config_dns);
      else dns.fromString("8.8.8.8");
      WiFi.config(ip, gw, sn, dns);
    }
  }
  WiFi.softAP("ESP-RGB-INVADERS", "12345678");
  server.begin();
}

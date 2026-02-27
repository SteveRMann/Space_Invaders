// --------------------------------------------------------------------------
// 3. HELPER FUNCTIONS
// --------------------------------------------------------------------------
CRGB hexToCRGB(String hex) {
  long number = strtol(&hex[1], NULL, 16);
  return CRGB((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
}

Melody parseSoundString(String data) {
  Melody m;
  if (data.length() == 0) return m;
  int start = 0;
  int end = data.indexOf(';');
  while (end != -1) {
    String pair = data.substring(start, end);
    int comma = pair.indexOf(',');
    if (comma != -1) {
      ToneCmd t;
      t.freq = pair.substring(0, comma).toInt();
      t.duration = pair.substring(comma + 1).toInt();
      m.push_back(t);
    }
    start = end + 1;
    end = data.indexOf(';', start);
  }
  String pair = data.substring(start);
  int comma = pair.indexOf(',');
  if (comma != -1) {
    ToneCmd t;
    t.freq = pair.substring(0, comma).toInt();
    t.duration = pair.substring(comma + 1).toInt();
    m.push_back(t);
  }
  return m;
}

void melodyFromStr(Melody &m, String s) {
  m = parseSoundString(s);
}


// --------------------------------------------------------------------------
// 5. GRAPHICS ENGINE
// --------------------------------------------------------------------------
CRGB getColor(int colorCode) {
  switch (colorCode) {
    case 1: return col_c1;
    case 2: return col_c2;
    case 3: return col_c3;
    case 4: return col_c4;
    case 5: return col_c5;
    case 6: return col_c6;
    case 7: return col_cw;
    default: return CRGB::Black;
  }
}

void drawCrispPixel(float pos, CRGB color) {
  int idx = round(pos);
  if (idx < 0 || idx >= config_num_leds) return;
  leds[idx + ledStartOffset] = color;
}

void flashPixel(int pos) {
  if (pos >= 0 && pos < config_num_leds) leds[pos + ledStartOffset] = CRGB::White;
}

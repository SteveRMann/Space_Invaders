// --------------------------------------------------------------------------
// 4. AUDIO ENGINE
// --------------------------------------------------------------------------
void playSound(SoundEvent evt) {
  Serial.print("playSound= ");
  Serial.println(evt);
  ///  if (!config_sound_on) return;
  ///  ///Added test for null argument
  ///  if (audioQueue == nullptr) {
  ///    // Optional: debug
  ///    Serial.println("playSound called but audioQueue is NULL");
  ///    return;
  ///  }
  ///  xQueueSend(audioQueue, &evt, 0);
}

void playShotSound(int color) {
  Serial.print("playShotSound= ");
  Serial.println(color);
}
///  switch (color) {
///    case 1: playSound(EVT_SHOT_BLUE); break;
///    case 2: playSound(EVT_SHOT_RED); break;
///    case 3: playSound(EVT_SHOT_GREEN); break;
///    case 7: playSound(EVT_SHOT_WHITE); break;
///    default: playSound(EVT_SHOT_BLUE); break;
///  }
///}

void playToneI2S(int freq, int durationMs) {
  if (freq <= 0) {
    size_t bytes_written;
    int samples = (SAMPLE_RATE * durationMs) / 1000;
    int16_t *buffer = (int16_t *)malloc(samples * 2);
    memset(buffer, 0, samples * 2);
    i2s_write(I2S_NUM_0, buffer, samples * 2, &bytes_written, portMAX_DELAY);
    free(buffer);
    return;
  }
  size_t bytes_written;
  int samples = (SAMPLE_RATE * durationMs) / 1000;
  int16_t *buffer = (int16_t *)malloc(samples * 2);
  int halfPeriod = SAMPLE_RATE / freq / 2;

  int16_t volume = map(config_volume_pct, 0, 100, 0, 10000);

  for (int i = 0; i < samples; i++) {
    buffer[i] = ((i / halfPeriod) % 2 == 0) ? volume : -volume;
  }
  i2s_write(I2S_NUM_0, buffer, samples * 2, &bytes_written, portMAX_DELAY);
  free(buffer);
}

void audioTask(void *parameter) {
  return;
}

/*
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  ///  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  ///  i2s_set_pin(I2S_NUM_0, &pin_config);
  ///  i2s_zero_dma_buffer(I2S_NUM_0);

  const Melody *currentMelody = nullptr;
  int noteIndex = 0;

  while (true) {
    SoundEvent newEvent;
    if (xQueueReceive(audioQueue, &newEvent, 0) == pdTRUE) {
      bool play = true;
      if ((currentMelody == &melWin || currentMelody == &melLose) && newEvent != EVT_START) {
        play = false;
      }
      if (play) {
        switch (newEvent) {
          case EVT_START: currentMelody = &melStart; break;
          case EVT_SHOT_BLUE: currentMelody = &melShotBlue; break;
          case EVT_SHOT_RED: currentMelody = &melShotRed; break;
          case EVT_SHOT_GREEN: currentMelody = &melShotGreen; break;
          case EVT_SHOT_WHITE: currentMelody = &melShotWhite; break;
          case EVT_MISTAKE: currentMelody = &melMistake; break;
          case EVT_HIT_SUCCESS: currentMelody = &melHit; break;
          case EVT_WIN: currentMelody = &melWin; break;
          case EVT_LOSE: currentMelody = &melLose; break;
          default: break;
        }
        noteIndex = 0;
        i2s_zero_dma_buffer(I2S_NUM_0);
      }
    }

    if (currentMelody != nullptr) {
      if (noteIndex >= currentMelody->size()) {
        currentMelody = nullptr;
        playToneI2S(0, 20);
        i2s_zero_dma_buffer(I2S_NUM_0);
      } else {
        ToneCmd t = (*currentMelody)[noteIndex];
        playToneI2S(t.freq, t.duration);
        noteIndex++;
      }
    } else {
      vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    // Prevent watchdog reset
    vTaskDelay(1);
  }
}
*/

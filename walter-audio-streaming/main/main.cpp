/**
 * @file main.cpp
 * @author Arnoud Devoogdt <arnoud@dptechnics.com>
 * @date 31 March 2026
 * @copyright DPTechnics bv
 * @brief Walter based audio streaming demo
 *
 * @section LICENSE
 *
 * Copyright (C) 2026, DPTechnics bv
 * All rights reserved.
 * 
  * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 * 
 * This file contains firmware for Walter that demonstrates streaming audio from
 * an internet radio station. It uses the walter-uc (Unified Communications)
 * library to manage network connectivity and the ESP32-audioI2S library to 
 * decode audio and output it through a WM8731 codec connected over I2S. 
 * The application is designed to be simple and focused on the streaming 
 * functionality, with automatic reconnection and bearer management handled by 
 * the underlying libraries.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Audio.h>
#include <bsp/walter.hpp>
#include <esp_log.h>

static const char* TAG = "walter_audio";

// ============================================================================
// Configuration — edit these for your deployment
// ============================================================================

#define WIFI_SSID "WIFI-SSID"
#define WIFI_PASSWORD "WIFI-PASSWORD"
#define CELL_APN "soracom.io"
#define STREAM_URL "https://stream.willstare.com:8450"

// ============================================================================
// Pin mapping
// ============================================================================

#define PIN_3V3_EN 0 // Active LOW — enables the 3V3OUT rail for peripherals

#define I2S_BCLK 17
#define I2S_LRCLK 10
#define I2S_DOUT 18
#define I2S_MCLK -1 // WM8731 supplies MCLK from its own crystal

#define I2C_SDA 8
#define I2C_SCL 9

// ============================================================================
// WM8731 codec
// ============================================================================

#define WM8731_ADDR 0x1A // I2C address with CSB pin LOW (0x1B if CSB HIGH)

/**
 * Write a 9-bit value to a WM8731 register.
 *
 * The I2C payload is two bytes:
 *   byte 0 — reg[6:0] | val[8]
 *   byte 1 — val[7:0]
 */
static bool wm8731_write(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(WM8731_ADDR);
  Wire.write((reg << 1) | ((val >> 8) & 0x01));
  Wire.write(val & 0xFF);
  return Wire.endTransmission() == 0;
}

/**
 * Initialise the WM8731 for I2S slave playback at 44.1 kHz.
 *
 * Power : mic, line-in, and ADC are powered down;
 *         DAC, headphone output, and oscillator are enabled.
 * Format: I2S, 32-bit word length, codec is bit-clock slave.
 * Clock : USB mode with 12 MHz crystal, SR=1000, BOSR=0 → 44.1 kHz.
 */
static void wm8731_init() {
  wm8731_write(0x0F, 0x000); // R15: reset
  delay(10);

  wm8731_write(0x06, 0x047); // R6:  power — mic/line-in/ADC off, DAC/out/osc on
  wm8731_write(0x02, 0x179); // R2:  left headphone  — 0 dB, load both (LRHPBOTH)
  wm8731_write(0x03, 0x179); // R3:  right headphone — 0 dB
  wm8731_write(0x04, 0x012); // R4:  analogue path   — DAC selected, no bypass
  wm8731_write(0x05, 0x000); // R5:  digital path    — no de-emphasis, no mute
  wm8731_write(0x07, 0x00E); // R7:  interface       — I2S, 32-bit, slave
  wm8731_write(0x08, 0x021); // R8:  sampling        — 44.1 kHz, USB mode

  if (!wm8731_write(0x09, 0x001)) { // R9: activate
    ESP_LOGE(TAG, "WM8731 not found on I2C — check wiring and 3.3 V supply");
  } else {
    ESP_LOGI(TAG, "WM8731 initialised");
  }
}

// ============================================================================
// Network
// ============================================================================

/**
 * Configure walter-uc drivers and block until a bearer is established.
 *
 * walter-uc tries each configured driver (Wi-Fi first, then cellular) and
 * keeps the selected bearer alive in the background. Returns false only if
 * no bearer could be brought up at all.
 */
static bool network_start() {
  CELL_DRV(uc.GM02S)->config(CELL_APN, 6);
  WIFI_DRV(uc.ESP_WIFI)->configStation(WIFI_SSID, WIFI_PASSWORD, 5);

  ESP_LOGI(TAG, "Starting Unified Comms...");
  if (!uc.controller.start()) {
    ESP_LOGE(TAG, "Could not connect — no bearer available");
    return false;
  }
  ESP_LOGI(TAG, "Network up");
  return true;
}

// ============================================================================
// Audio streaming
// ============================================================================

static Audio* audio = nullptr;

/**
 * One-time hardware and software initialisation.
 *
 * Sequence:
 *   1. Enable 3V3OUT rail (powers the WM8731 proto board).
 *   2. Initialise WM8731 over I2C.
 *   3. Bring up the network via walter-uc.
 *   4. Create and configure the Audio engine.
 *
 * Returns false if any step fails, in which case the stream task exits.
 */
static bool audio_setup() {
  pinMode(PIN_3V3_EN, OUTPUT);
  digitalWrite(PIN_3V3_EN, LOW);
  delay(50); // allow 3V3OUT to stabilise before accessing peripherals

  ESP_LOGI(TAG, "Walter Audio Streaming");
  ESP_LOGI(TAG, "PSRAM: %lu bytes", (unsigned long)ESP.getPsramSize());

  Wire.begin(I2C_SDA, I2C_SCL);
  wm8731_init();

  if (!network_start()) {
    return false;
  }

  audio = new Audio();
  Audio::audio_info_callback = [](Audio::msg_t m) {
    ESP_LOGI(TAG, "[%s] %s", m.s, m.msg);
  };
  audio->setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT, I2S_MCLK);
  audio->setVolume(15); // 0..21
  return true;
}

/**
 * Stream task: connects to STREAM_URL and drives the audio decode loop.
 *
 * Reconnects automatically whenever the stream ends or drops.
 * Pinned to Core 1, leaving Core 0 free for the Wi-Fi / IP stack.
 */
static void audio_stream_task(void*) {
  if (!audio_setup()) {
    ESP_LOGE(TAG, "Initialisation failed — task exiting");
    vTaskDelete(NULL);
    return;
  }

  for (;;) {
    ESP_LOGI(TAG, "Connecting to: %s", STREAM_URL);
    if (!audio->connecttohost(STREAM_URL)) {
      ESP_LOGE(TAG, "connecttohost failed — retrying in 5 s");
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    while (audio->isRunning()) {
      audio->loop();
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGW(TAG, "Stream ended — reconnecting in 3 s");
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// ============================================================================
// Entry point
// ============================================================================

extern "C" void app_main(void) {
  initArduino();
  xTaskCreatePinnedToCore(audio_stream_task, "audio_stream", 16384, NULL, 1, NULL, 1);
}

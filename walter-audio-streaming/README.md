# Walter Audio Streaming

This demo shows how to stream internet radio on the [DPTechnics Walter](https://www.dptechnics.com/walter)
IoT module with automatic bearer selection. The [walter-uc](https://github.com/QuickSpot/walter-uc)
Unified Communications library evaluates all available radio interfaces (Wi-Fi and LTE-M/NB-IoT),
picks the best one, and keeps it alive in the background. From the application's perspective
there is nothing more to do than open a URL — walter-uc handles the rest.

Audio is decoded in software using [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)
and output through a WM8731 codec connected over I2S (data) and I2C (control).

---

## Hardware

| Part | Notes |
|------|-------|
| DPTechnics Walter (ESP32-S3) | Target board |
| WM8731 codec breakout | Connected via I2S + I2C |

### Pin connections

| Signal | GPIO | Description |
|--------|------|-------------|
| I2S BCLK | 17 | Bit clock |
| I2S LRCLK | 10 | Left/right clock |
| I2S DOUT | 18 | Serial audio data |
| I2S MCLK | — | Supplied by WM8731's onboard crystal |
| I2C SDA | 8 | Codec control bus |
| I2C SCL | 9 | Codec control bus |
| 3V3OUT enable | GPIO0 (active LOW) | Powers the codec proto board |

---

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/) ≥ 5.4.0
- Xtensa toolchain (installed alongside ESP-IDF)

---

## Getting started

### 1 — Clone

```bash
git clone https://github.com/QuickSpot/walter-example-projects.git
cd walter-audio-streaming
```

### 2 — Configure credentials

Edit `main/main.cpp` and set your network credentials and stream URL:

```cpp
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"
#define CELL_APN      "your.apn"          // e.g. "soracom.io"
#define STREAM_URL    "https://your-stream-url"
```

### 3 — Build and flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On Windows replace `/dev/ttyUSB0` with the appropriate COM port (e.g. `COM3`).

---

## Project structure

```
.
├── main/
│   ├── main.cpp               # Application source
│   ├── CMakeLists.txt
│   └── idf_component.yml      # Managed dependency: arduino-esp32
├── components/
│   ├── ESP32-audioI2S/        # Vendored — schreibfaul1/ESP32-audioI2S @ v3.4.5 (custom IDF CMakeLists.txt)
│   └── walter-uc/             # Vendored — QuickSpot/walter-uc @ v0.1.0
├── sdkconfig.defaults          # Default build configuration (PSRAM, flash size, …)
├── partitions.csv              # Custom partition table (3 MB factory app)
└── CMakeLists.txt
```

---

## Dependencies

| Component | Source | Version | License |
|-----------|--------|---------|---------|
| [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) | vendored (custom IDF CMakeLists.txt) | 3.4.5 | GPL-3.0 |
| [walter-uc](https://github.com/QuickSpot/walter-uc) | vendored | 0.1.0 | DPTechnics 5-clause |
| [espressif/arduino-esp32](https://components.espressif.com/components/espressif/arduino-esp32) | IDF Component Registry | ≥3.0.0 | Apache-2.0 |
| [espressif/esp_modem](https://components.espressif.com/components/espressif/esp_modem) | IDF Component Registry | ≥1.0.0 | Apache-2.0 |

---

## License

This project is licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE).

The GPL-3.0 applies because the project links against
[ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) (GPL-3.0).
The `walter-uc` component is separately licensed under the
[DPTechnics 5-clause license](components/walter-uc/LICENSE)
and may only be used with Walter hardware from DPTechnics bv.

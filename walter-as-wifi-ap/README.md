# Walter as a Wi-Fi Access Point

This example turns a [DPTechnics Walter](https://www.dptechnics.com/walter) module into a cellular Wi-Fi hotspot. The onboard Sequans GM02SP LTE modem provides the internet uplink via a PPP over Serial (PPPoS) link. The ESP32-S3 creates a Wi-Fi soft AP and uses IP NAT to route traffic from connected Wi-Fi clients through the LTE connection. DNS information received from the LTE network is forwarded to clients via the integrated DHCP server. The firmware monitors the PPP link and reconnects automatically on loss of connectivity, with signal-quality checks and a modem hard-reset as a last resort.

This example is based on the [ap_to_pppos](https://github.com/espressif/esp-protocols/tree/master/components/esp_modem/examples/ap_to_pppos) example from the Espressif esp-modem repository, modified to target the Walter hardware and its Sequans GM02SP modem.

---

## Limitations

Keep in mind that LTE-M and NB-IoT are low-bandwidth cellular technologies with typical throughputs of a few hundred Kbps and higher latencies. This makes the example only suited for use cases such as IoT data, remote administration, and some light internet use.

The UART between the ESP32-S3 and the GM02SP runs at 115200 baud by default, which can be a bottleneck in this example. Increasing the modem's UART baudrate using `AT+SQNHWCFG` and setting the matching baudrate in `network_dce.c` can increase the achievable speed:

```c
dte_config.uart_config.baud_rate = 921600;
```

---

## Hardware

| Part | Notes |
|------|-------|
| DPTechnics Walter (ESP32-S3) | Target board |
| LTE antenna | Connected to Walter's U.FL connector |
| LTE-M or NB-IoT SIM card | Inserted in Walter's SIM slot |
| USB-C cable | For power and flashing |

### Default pin assignment

These match the Walter board layout and are the Kconfig defaults. They can be changed via `idf.py menuconfig` → *Example Configuration*.

| Signal | GPIO |
|--------|------|
| Modem TX  | 48 |
| Modem RX  | 14 |
| Modem CTS | 47 |
| Modem RTS | 21 |
| Modem RESET | 45 |

---

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/) ≥ 5.0.0

---

## Getting started

### 1 — Clone

```bash
git clone https://github.com/QuickSpot/walter-example-projects.git
cd walter-as-wifi-ap
```

### 2 — Configure

```bash
idf.py set-target esp32s3
idf.py menuconfig
```

Navigate to **Example Configuration** and set:

| Option | Description |
|--------|-------------|
| WiFi SSID | SSID advertised by the soft AP |
| WiFi Password | WPA2 password (leave empty for open network) |
| WiFi Channel | 802.11 channel (1–13) |
| Maximal STA connections | Maximum number of simultaneous Wi-Fi clients |
| Set MODEM APN | APN of your mobile operator, leave blank to autodetect |
| SIM PIN needed | Enable if your SIM requires a PIN at startup |
| Set SIM PIN | Four-digit PIN (only shown when the option above is enabled) |

### 3 — Build and flash

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

On Windows replace `/dev/ttyACM0` with the appropriate COM port (e.g. `COM3`).

---

## How it works

1. Walter initialises the GM02SP modem over UART with hardware flow control and resets it via the RESET line.
2. A PPPoS network interface is created in lwIP and the modem is commanded into data mode.
3. Once the PPP link receives an IP address the firmware starts the Wi-Fi soft AP and enables NAPT on the AP interface so that client traffic is masqueraded behind Walter's LTE-assigned IP.
4. The DNS server address from the LTE link is injected into the DHCP server so clients receive working name resolution automatically.
5. A background loop waits for a `PPP_LOST_IP` event. On disconnect it exits data mode, checks signal quality, and re-establishes the link.

---

## Project structure

```
.
├── main/
│   ├── ap_to_pppos.c          # app_main, Wi-Fi AP init, PPPoS lifecycle, NAT setup
│   ├── network_dce.c          # Modem DCE wrapper (init, reset, start/stop network, signal check)
│   ├── network_dce.h          # Public modem API
│   ├── Kconfig.projbuild      # menuconfig options (SSID, APN, pins, …)
│   ├── idf_component.yml      # Managed dependency: espressif/esp_modem ^2
│   └── CMakeLists.txt
├── sdkconfig.defaults         # PPP, NAPT, and UART ISR defaults
└── CMakeLists.txt
```

---

## Dependencies

| Component | Source | Version | License |
|-----------|--------|---------|---------|
| [espressif/esp_modem](https://components.espressif.com/components/espressif/esp_modem) | IDF Component Registry | ^2 | Apache-2.0 |
| ESP-IDF (lwIP PPPoS + NAPT) | Built-in | ≥ 5.0.0 | Apache-2.0 |

---

## License

This project is licensed under the **GNU General Public License v3.0**.  
See [https://www.gnu.org/licenses/](https://www.gnu.org/licenses/) for the full text.

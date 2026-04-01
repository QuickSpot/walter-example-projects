# Walter Unified Communications Library

## Introduction

Walter is a multi-radio IoT module combining an **Espressif ESP32-S3** with a **Sequans GM02SP** cellular modem. Out of the box, a Walter module supports:

* **Cellular**: LTE Cat-M1, LTE Cat-NB1, LTE Cat-NB2
* **Wi-Fi**
* **Bluetooth Low Energy**
* **GNSS**: GPS and GLONASS

## Unified Communications

The **Walter Unified Communications library** provides a high-level networking abstraction across multiple physical interfaces. Instead of targeting a specific modem or Wi-Fi stack, `walter-uc` introduces **drivers**:

* **Cellular driver** – based on PPP via the ESP-IDF `esp-modem` library (GM02SP)
* **Wi-Fi driver** – based on ESP-IDF Wi-Fi stack
* **Future drivers** – e.g. Ethernet, or other transport integrations

Each driver contributes a network interface. `walter-uc` monitors the state of all drivers and automatically selects the **most favorable** one according to priority and availability.

From the application perspective, this means you can continue to use the **standard ESP-IDF networking APIs** (`sockets`, `MQTT`, `HTTP`, etc.) without worrying about which underlying transport is currently active.

## Key Features

* **Seamless driver selection**
  Transparent switching between Wi-Fi, LTE-M/NB-IoT, and future interfaces like Ethernet.

* **Transparent error handling**
  Built-in recovery and retry mechanisms per driver.

* **Protocol independence**
  Applications keep using standard APIs (`socket`, `MQTT`, `HTTP`, etc.), regardless of the active transport.

* **Certificate management**
  Unified handling of TLS certificates across drivers.

## Application Responsibility

While `walter-uc` ensures there is always an active, working network interface, **connection continuity remains the responsibility of the application**.

Switching between drivers may result in a **new IP address**, which invalidates existing higher-level sessions. Your application must:

* Detect when the active driver changes
* Re-establish sessions (e.g. MQTT, TCP sockets, CoAP, …) as necessary

This design provides robustness and flexibility while maintaining compatibility with the ESP-IDF networking stack.

## Repository Structure

```
C:.
│   .clang-format
│   .gitignore
│   CMakeLists.txt
│   idf_component.yml
│   Kconfig
│   README.md
│
├───docs
│       Architecture.pdf
│
├───examples
│   ├───bluecherry
│   ├───mqtt
│   └───udp-socket
│
└───src
    │   uc.cpp
    │   uc.hpp
    │   utils.h
    │
    ├───bsp
    └───driver
```

* **docs/** – Reference architecture documentation
* **examples/** – Example applications (BlueCherry, MQTT, UDP socket)
* **src/** – Core library, board support, and drivers
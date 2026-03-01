# esphome-bm6_ble

ESPHome external component for the **BM6 Battery Monitor** (and compatible BM200 models) using BLE communication. Optimized for **ESP32-S3** and the **ESP-IDF** framework.

This component follows a **Type-based entity pattern** (similar to Dometic CFX components), allowing you to define individual sensors for voltage, temperature, and status alerts within a single hub.

## Features

- **Voltage Monitoring**: Real-time battery voltage ($0.01V$ precision).
- **Temperature Sensing**: Internal battery temperature reporting in °C.
- **Battery Percentage (SoC)**: State of Charge calculation ($0-100\%$).
- **Status Indicators**: Binary sensors for **Charging**, and **Low Voltage**
- **NimBLE Support**: Uses the memory-efficient NimBLE stack for better performance on the S3.

## Board & ESPHome Version

- **Board**: ESP32-S3 (e.g., Lolin S3 Pro, DevKitC-1).
- **ESPHome**: 2026.2.0 or newer.
- **Framework**: ESP-IDF.

## Installation

Add this to your ESPHome configuration:

```yaml
external_components:
  - source: github://andrewbackway/esphome-bm6_ble@main

```

Configuration
-------------

### 1\. Global Bluetooth Setup

The `esp32_ble_tracker` discovers the device, while `ble_client` manages the authenticated connection.

YAML

```
esp32:
  board: lolin_s3_pro
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_BT_NIMBLE_ENABLED: y

esp32_ble_tracker:
  scan_parameters:
    active: true

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF" # Replace with your BM6 MAC
    id: bm6_ble

bm6_ble:
  id: bm6_1
  ble_client_id: bm6_ble

```

### 2\. Sensor Configuration

Define each entity using the `type` field to link it to the BM6 data stream.

#### Numeric Sensors (`sensor`)

| **Type** | **Description** | **Unit** | **Default Icon** |
| --- | --- | --- | --- |
| `VOLTAGE` | Battery voltage | V | `mdi:flash` |
| `TEMPERATURE` | Battery temperature | °C | `mdi:thermometer` |
| `BATTERY_LEVEL` | State of Charge | % | `mdi:battery` |

YAML

```
sensor:
  - platform: bm6_ble
    bm6_ble_id: bm6_1
    type: VOLTAGE
    name: "Battery Voltage"

  - platform: bm6_ble
    bm6_ble_id: bm6_1
    type: BATTERY_LEVEL
    name: "Battery SoC"

  - platform: bm6_ble
    bm6_ble_id: bm6_1
    type: BATTERY_LEVEL
    name: "Battery SoC"
```

#### Binary Sensors (`binary_sensor`)

| **Type** | **Description** | **Device Class** |
| --- | --- | --- |
| `CHARGING` | Battery is charging | `battery_charging` |
| `LOW_VOLTAGE` | Voltage below threshold | `problem` |

YAML

```
binary_sensor:
  - platform: bm6_ble
    bm6_ble_id: bm6_hub
    type: CHARGING
    name: "Battery Charging"

  - platform: bm6_ble
    bm6_ble_id: bm6_1
    type: LOW_VOLTAGE
    name: "Low Voltage Alert"

```

License
-------

MIT License

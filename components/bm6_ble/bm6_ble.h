#pragma once
#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace bm6_ble {

class BM6Hub : public Component, public ble_client::BLEClientNode {
 public:
  BM6Hub(); // Add this line here
  void dump_config() override;
  void setup() override;
  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) override;

  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { temperature_sensor_ = s; }
  void set_level_sensor(sensor::Sensor *s) { level_sensor_ = s; }
  
  void set_low_voltage_binary(binary_sensor::BinarySensor *s) { low_volt_binary_ = s; }
  void set_weak_battery_binary(binary_sensor::BinarySensor *s) { weak_battery_binary_ = s; }
  void set_charging_binary(binary_sensor::BinarySensor *s) { charging_binary_ = s; }

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *level_sensor_{nullptr};
  
  binary_sensor::BinarySensor *low_volt_binary_{nullptr};
  binary_sensor::BinarySensor *weak_battery_binary_{nullptr};
  binary_sensor::BinarySensor *charging_binary_{nullptr};
  
  uint16_t char_handle_write_{0};
  uint16_t char_handle_notify_{0};
  
  // New state tracking to prevent race conditions on the S3
  bool subscribed_{false};
};

} // namespace bm6_ble
} // namespace esphome
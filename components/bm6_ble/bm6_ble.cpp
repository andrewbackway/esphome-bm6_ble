#include "bm6_ble.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bm6_ble {

static const char *const TAG = "bm6_ble";

// The specific BM6 Auth Key
static const uint8_t AUTH_KEY[16] = {0x69, 0x7e, 0xa0, 0xb5, 0xd5, 0x4c, 0xf0, 0x24, 0xe7, 0x94, 0x77, 0x23, 0x55, 0x55, 0x41, 0x14};

BM6Hub::BM6Hub() {
    ESP_LOGI(TAG, "BM6Hub constructor called - object instantiated!");
}

void BM6Hub::dump_config() {
    ESP_LOGCONFIG(TAG, "BM6 BLE Hub:");
    if (this->parent_) {
        ESP_LOGCONFIG(TAG, "  BLE Client ID: present");
    } else {
        ESP_LOGCONFIG(TAG, "  BLE Client: MISSING!");
    }
    ESP_LOGCONFIG(TAG, "  State: %s", this->state_str().c_str());  // or similar
}

void BM6Hub::setup() {
    ESP_LOGI(TAG, "BM6 Hub Setup Initialized");
}

void BM6Hub::loop() {
    // Structural fix from bm2_ble: don't rush the handshake in the event handler
    if (!this->subscribed_ && this->parent_ != nullptr && this->parent_->connected()) {
        auto svc = this->parent_->get_service(0xfff0);
        if (svc != nullptr) {
            auto char_write = svc->get_characteristic(0xfff3);
            auto char_notify = svc->get_characteristic(0xfff4);

            if (char_write && char_notify) {
                this->char_handle_write_ = char_write->handle;
                this->char_handle_notify_ = char_notify->handle;

                // 1. Authenticate first to unlock the device
                esp_ble_gattc_write_char(this->parent()->get_gattc_if(), 
                                        this->parent()->get_conn_id(), 
                                        this->char_handle_write_, 16, (uint8_t *)AUTH_KEY, 
                                        ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);

                // 2. Register for notifications
                esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), 
                                                 this->parent()->get_remote_bda(), 
                                                 this->char_handle_notify_);
                
                this->subscribed_ = true;
                ESP_LOGI(TAG, "BM6 Authenticated and Subscribed");
            }
        }
    }
}

void BM6Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_NOTIFY_EVT && param->notify.handle == this->char_handle_notify_) {
    // Protocol parsing for BM6 (Non-encrypted bytes)
    if (param->notify.value_len < 6) return;

    float volt = (uint16_t)(param->notify.value[2] << 8 | param->notify.value[1]) / 100.0f;
    float temp = (int8_t)param->notify.value[3];
    float level = param->notify.value[5]; // SoC
    
    // Status Flags
    bool low_v = (param->notify.value[4] & 0x01);
    bool weak_b = (param->notify.value[4] & 0x02);
    bool charging = (param->notify.value[4] & 0x04);

    if (this->voltage_sensor_) this->voltage_sensor_->publish_state(volt);
    if (this->temperature_sensor_) this->temperature_sensor_->publish_state(temp);
    if (this->level_sensor_) this->level_sensor_->publish_state(level);
    
    if (this->low_volt_binary_) this->low_volt_binary_->publish_state(low_v);
    if (this->weak_battery_binary_) this->weak_battery_binary_->publish_state(weak_b);
    if (this->charging_binary_) this->charging_binary_->publish_state(charging);
  }
}

} // namespace bm6_ble
} // namespace esphome
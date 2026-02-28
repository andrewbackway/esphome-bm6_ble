#include "bm6_ble.h"
#include "esphome/core/log.h"
#include <aes/esp_aes.h>

namespace esphome {
namespace bm6_ble {

static const char *const TAG = "bm6_ble";
// Correct AES key
static const uint8_t AUTH_KEY[16] = {0x69, 0x7e, 0xa0, 0xb5, 0xd5, 0x4c, 0xf0, 0x24, 0xe7, 0x94, 0x77, 0x23, 0x55, 0x55, 0x41, 0x14};
/static const uint8_t AES_KEY[16] = {108, 101, 97, 103, 101, 110, 100, 255, 254, 48, 49, 48, 48, 48, 48, 57};
// Command to start notifications (plain, before encrypt)
static const uint8_t START_CMD[16] = {0xd1, 0x55, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

BM6Hub::BM6Hub() {
    ESP_LOGI(TAG, "BM6Hub constructor called - object instantiated!");
}

void BM6Hub::dump_config() { /* your existing */ }

void BM6Hub::setup() {
    ESP_LOGI(TAG, "BM6 Hub Setup Initialized");
}

void BM6Hub::loop() {
    // Remove all handshake logic from here - moved to event handler
}

// ... includes and statics unchanged

void BM6Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    ESP_LOGD(TAG, "GATTC event: %d", event);

    switch (event) {
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Connection opened successfully (conn_id=%d)", param->open.conn_id);
                esp_ble_gattc_search_service(gattc_if, param->open.conn_id, nullptr);  // Start discovery
            } else {
                ESP_LOGE(TAG, "Connection failed, status=0x%02x", param->open.status);
            }
            break;

        case ESP_GATTC_SEARCH_CMPL_EVT:
            if (param->search_cmpl.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Service discovery complete");
                if (this->subscribed_) return;  // Already done

                auto svc = this->parent_->get_service(0xfff0);
                if (svc == nullptr) {
                    ESP_LOGE(TAG, "FFF0 service not found");
                    return;
                }

                auto char_write = svc->get_characteristic(0xfff3);
                auto char_notify = svc->get_characteristic(0xfff4);

                if (char_write && char_notify) {
                    this->char_handle_write_ = char_write->handle;
                    this->char_handle_notify_ = char_notify->handle;

                    // Auth with raw key (your original)
                    esp_ble_gattc_write_char(gattc_if, param->search_cmpl.conn_id,
                                             this->char_handle_write_, 16, (uint8_t *)AUTH_KEY,
                                             ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                    ESP_LOGI(TAG, "Raw auth key sent");

                    // Enable CCCD (critical!)
                    uint8_t notify_en[2] = {0x01, 0x00};
                    auto cccd = char_notify->get_descriptor(0x2902);
                    if (cccd) {
                        esp_ble_gattc_write_char_descr(gattc_if, param->search_cmpl.conn_id,
                                                       cccd->handle, 2, notify_en,
                                                       ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                        ESP_LOGI(TAG, "CCCD enabled");
                    } else {
                        ESP_LOGE(TAG, "CCCD not found - notifications may fail");
                    }

                    // Register (fallback, some stacks need both)
                    esp_ble_gattc_register_for_notify(gattc_if, this->parent_->get_remote_bda(),
                                                      this->char_handle_notify_);

                    this->subscribed_ = true;
                    ESP_LOGI(TAG, "Handshake complete");
                } else {
                    ESP_LOGE(TAG, "Characteristics not found");
                }
            }
            break;

        case ESP_GATTC_WRITE_EVT:
            ESP_LOGD(TAG, "Write response for handle 0x%04x, status=0x%02x", param->write.handle, param->write.status);
            if (param->write.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "Write FAILED - check if auth rejected");
            }
            break;

        case ESP_GATTC_NOTIFY_EVT:
            if (param->notify.handle == this->char_handle_notify_ && param->notify.value_len == 16) {
                ESP_LOGI(TAG, "Encrypted notification received (len=16)");

                uint8_t decrypted[16] = {0};
                uint8_t iv[16] = {0};  // zero IV

                mbedtls_aes_context aes;
                mbedtls_aes_init(&aes);
                mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
                mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, param->notify.value, decrypted);
                mbedtls_aes_free(&aes);

                // Log raw decrypted for debug (hex)
                char hex_buf[50];
                format_hex_pretty(decrypted, 16, hex_buf, sizeof(hex_buf));
                ESP_LOGD(TAG, "Decrypted: %s", hex_buf);

                // Check prefix (common sanity check)
                if (decrypted[0] != 0xd1 || decrypted[1] != 0x55 || decrypted[2] != 0x07) {
                    ESP_LOGW(TAG, "Invalid decrypted prefix - wrong key or variant?");
                    return;
                }

                // Parse (from tarball.ca + Goodwillson/NorbertRoller code)
                float volt = ((decrypted[8] << 8) | decrypted[7]) / 100.0f;
                int8_t temp_sign = (decrypted[3] == 0x01) ? -1 : 1;
                float temp = temp_sign * decrypted[4];
                float level = decrypted[6];  // SoC %

                uint8_t flags = decrypted[5];
                bool low_v = (flags & 0x01);
                bool weak_b = (flags & 0x02);
                bool charging = (flags & 0x04);

                // Publish
                if (this->voltage_sensor_) this->voltage_sensor_->publish_state(volt);
                if (this->temperature_sensor_) this->temperature_sensor_->publish_state(temp);
                if (this->level_sensor_) this->level_sensor_->publish_state(level);

                if (this->low_volt_binary_) this->low_volt_binary_->publish_state(low_v);
                if (this->weak_battery_binary_) this->weak_battery_binary_->publish_state(weak_b);
                if (this->charging_binary_) this->charging_binary_->publish_state(charging);

                ESP_LOGD(TAG, "Parsed: Voltage=%.2f V, Temp=%.1f °C, SoC=%.0f%%, Flags=0x%02X", volt, temp, level, flags);
            }
            break;

        case ESP_GATTC_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Disconnected");
            this->subscribed_ = false;
            break;

        default:
            break;
    }
}

}  // namespace bm6_ble
}  // namespace esphome
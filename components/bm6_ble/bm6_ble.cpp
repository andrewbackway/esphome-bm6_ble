#include "bm6_ble.h"
#include "esphome/core/log.h"
#include "esp_aes.h"  // For AES decryption

namespace esphome {
namespace bm6_ble {

static const char *const TAG = "bm6_ble";
// Correct AES key
static const uint8_t AES_KEY[16] = {108, 101, 97, 103, 101, 110, 100, 255, 254, 48, 49, 48, 48, 48, 48, 57};
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

void BM6Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    ESP_LOGD(TAG, "GATTC event: %d", event);

    switch (event) {
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Connection opened (conn_id=%d)", param->open.conn_id);
                esp_ble_gattc_search_service(gattc_if, param->open.conn_id, nullptr);  // Discover services
            } else {
                ESP_LOGE(TAG, "Connect failed: status=%d", param->open.status);
            }
            break;

        case ESP_GATTC_SEARCH_CMPL_EVT:
            if (param->search_cmpl.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "Service discovery complete");
                auto *svc = this->parent_->get_service(0xFFF0);
                if (svc == nullptr) {
                    ESP_LOGE(TAG, "Service 0xFFF0 not found");
                    return;
                }
                auto *chr_w = svc->get_characteristic(0xFFF3);
                auto *chr_n = svc->get_characteristic(0xFFF4);
                if (chr_w == nullptr || chr_n == nullptr) {
                    ESP_LOGE(TAG, "Characteristics missing");
                    return;
                }
                this->char_handle_write_ = chr_w->handle;
                this->char_handle_notify_ = chr_n->handle;

                // Encrypt the start command
                uint8_t encrypted_cmd[16];
                esp_aes_context aes_ctx;
                esp_aes_init(&aes_ctx);
                esp_aes_setkey(&aes_ctx, AES_KEY, 128);  // 128-bit key
                esp_aes_crypt_cbc(&aes_ctx, ESP_AES_ENCRYPT, 16, (uint8_t[16]){0}, (uint8_t*)START_CMD, encrypted_cmd);
                esp_aes_free(&aes_ctx);

                // Write encrypted command to start notifications
                esp_ble_gattc_write_char(gattc_if, param->search_cmpl.conn_id, this->char_handle_write_,
                                         16, encrypted_cmd, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                ESP_LOGI(TAG, "Start command sent");

                // Enable notifications via CCCD
                uint8_t enable_notify[2] = {0x01, 0x00};
                auto *cccd = chr_n->get_descriptor(0x2902);
                if (cccd) {
                    esp_ble_gattc_write_char_descr(gattc_if, param->search_cmpl.conn_id, cccd->handle,
                                                   2, enable_notify, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                    ESP_LOGI(TAG, "CCCD enabled for notifications");
                } else {
                    ESP_LOGE(TAG, "CCCD descriptor not found");
                }

                this->subscribed_ = true;
            }
            break;

        case ESP_GATTC_NOTIFY_EVT:
            if (param->notify.handle == this->char_handle_notify_ && param->notify.value_len == 16) {  // Encrypted len=16
                uint8_t decrypted[16];
                esp_aes_context aes_ctx;
                esp_aes_init(&aes_ctx);
                esp_aes_setkey(&aes_ctx, AES_KEY, 128);
                esp_aes_crypt_cbc(&aes_ctx, ESP_AES_DECRYPT, 16, (uint8_t[16]){0}, param->notify.value, decrypted);
                esp_aes_free(&aes_ctx);

                // Check prefix (d1 55 07)
                if (decrypted[0] == 0xd1 && decrypted[1] == 0x55 && decrypted[2] == 0x07) {
                    float volt = ((decrypted[8] << 8) | decrypted[7]) / 100.0f;
                    int temp = (decrypted[3] == 0x01) ? -decrypted[4] : decrypted[4];  // Signed temp
                    float level = decrypted[6];  // SoC %

                    // Flags (from repos - matches your bits)
                    bool low_v = (decrypted[5] & 0x01);
                    bool weak_b = (decrypted[5] & 0x02);
                    bool charging = (decrypted[5] & 0x04);

                    // Publish
                    if (voltage_sensor_) voltage_sensor_->publish_state(volt);
                    if (temperature_sensor_) temperature_sensor_->publish_state(temp);
                    if (level_sensor_) level_sensor_->publish_state(level);
                    if (low_volt_binary_) low_volt_binary_->publish_state(low_v);
                    if (weak_battery_binary_) weak_battery_binary_->publish_state(weak_b);
                    if (charging_binary_) charging_binary_->publish_state(charging);

                    ESP_LOGD(TAG, "Parsed: Volt=%.2fV, Temp=%dC, SoC=%.0f%%", volt, temp, level);
                } else {
                    ESP_LOGW(TAG, "Invalid decrypted prefix");
                }
            }
            break;

        case ESP_GATTC_DISCONNECT_EVT:
            subscribed_ = false;
            ESP_LOGI(TAG, "Disconnected");
            break;

        default:
            break;
    }
}

}  // namespace bm6_ble
}  // namespace esphome
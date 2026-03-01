#include "bm6_ble.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bm6_ble {

static const char *const TAG = "bm6_ble";

// BM6 AES-128-CBC key (from protocol reverse engineering, confirmed via test.py)
// Plaintext: "eagend" + {0xFF, 0xFE, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x39}
static const uint8_t DEVICE_KEY[16] = {
    0x6c, 0x65, 0x61, 0x67, 0x65, 0x6e, 0x64, 0xff,
    0xfe, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x39
};

// Trigger command that tells the BM6 to start sending volt/temp notifications
// Matches test.py: encrypt(bytearray.fromhex("d1550700000000000000000000000000"))
static const uint8_t CMD_PLAINTEXT[16] = {
    0xd1, 0x55, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// AES-128-CBC encrypt with a zero IV (matches test.py behaviour)
static void aes_encrypt(const uint8_t *key, const uint8_t *plaintext, uint8_t *out) {
    esp_aes_context ctx;
    esp_aes_init(&ctx);
    esp_aes_setkey(&ctx, key, 128);
    uint8_t iv[16] = {0};
    esp_aes_crypt_cbc(&ctx, ESP_AES_ENCRYPT, 16, iv, plaintext, out);
    esp_aes_free(&ctx);
}

// AES-128-CBC decrypt with a zero IV (matches test.py behaviour)
static void aes_decrypt(const uint8_t *key, const uint8_t *ciphertext, uint8_t *out) {
    esp_aes_context ctx;
    esp_aes_init(&ctx);
    esp_aes_setkey(&ctx, key, 128);
    uint8_t iv[16] = {0};
    esp_aes_crypt_cbc(&ctx, ESP_AES_DECRYPT, 16, iv, ciphertext, out);
    esp_aes_free(&ctx);
}

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
}

void BM6Hub::setup() {
    ESP_LOGI(TAG, "BM6 Hub Setup Initialized");
}

void BM6Hub::loop() {
    if (!this->subscribed_ && this->parent_->connected()) {
        auto *svc = this->parent_->get_service(0xfff0);
        if (svc == nullptr) return;

        auto *chr_w = svc->get_characteristic(0xfff3);
        auto *chr_n = svc->get_characteristic(0xfff4);

        if (chr_w != nullptr && chr_n != nullptr) {
            this->char_handle_write_ = chr_w->handle;
            this->char_handle_notify_ = chr_n->handle;

            // Encrypt the trigger command and write it to FFF3
            // Matches test.py: client.write_gatt_char("FFF3", encrypt(...), response=True)
            uint8_t encrypted_cmd[16];
            aes_encrypt(DEVICE_KEY, CMD_PLAINTEXT, encrypted_cmd);

            ESP_LOGD(TAG, "Sending encrypted trigger command to FFF3");
            esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                     this->char_handle_write_, 16, encrypted_cmd,
                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);

            // Subscribe to FFF4 notifications
            // Matches test.py: client.start_notify("FFF4", notification_handler)
            esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                              this->char_handle_notify_);

            this->subscribed_ = true;
            ESP_LOGI(TAG, "Encrypted trigger sent and notifications subscribed for BM6");
        }
    }
}

void BM6Hub::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    if (event == ESP_GATTC_DISCONNECT_EVT) {
        this->subscribed_ = false;
        ESP_LOGD(TAG, "Disconnected - resetting subscription flag");
        return;
    }

    if (event == ESP_GATTC_OPEN_EVT) {
        if (param->open.status == ESP_GATT_OK) {
            ESP_LOGI(TAG, "Connection opened successfully!");
        } else {
            ESP_LOGE(TAG, "Connection failed, status=%d", param->open.status);
        }
    }

    if (event == ESP_GATTC_NOTIFY_EVT && param->notify.handle == this->char_handle_notify_) {
        if (param->notify.value_len < 16) {
            ESP_LOGW(TAG, "Notification too short (%d bytes), ignoring", param->notify.value_len);
            return;
        }

        // Decrypt the 16-byte AES-CBC payload
        // Matches test.py: cipher.decrypt(data).hex()
        uint8_t d[16];
        aes_decrypt(DEVICE_KEY, param->notify.value, d);

        // Validate the d15507 header — same check as test.py: message[0:6] == "d15507"
        if (d[0] != 0xd1 || d[1] != 0x55 || d[2] != 0x07) {
            ESP_LOGV(TAG, "Ignoring notification with unknown header: %02X %02X %02X", d[0], d[1], d[2]);
            return;
        }

        // Temperature
        // test.py: message[6:8] == "01" → negative sign; message[8:10] → magnitude
        // d[3] = sign byte, d[4] = magnitude byte
        float temp = (float) d[4];
        if (d[3] == 0x01)
            temp = -temp;

        // SoC
        // test.py: int(message[12:14], 16) → d[6]
        float level = (float) d[6];

        // Voltage
        // test.py: int(message[15:18], 16) / 100
        // message[15:18] spans the low nibble of d[7] and all of d[8] (3 hex nibbles = 12 bits)
        float volt = (float)(((d[7] & 0x0F) << 8) | d[8]) / 100.0f;

        ESP_LOGD(TAG, "BM6 data: %.2fV  %.0f°C  %.0f%%", volt, temp, level);

        if (this->voltage_sensor_)     this->voltage_sensor_->publish_state(volt);
        if (this->temperature_sensor_) this->temperature_sensor_->publish_state(temp);
        if (this->level_sensor_)       this->level_sensor_->publish_state(level);

        // Binary sensor flags from d[5] — byte position unverified, to be confirmed later
        bool low_v    = (d[5] & 0x01);
        bool weak_b   = (d[5] & 0x02);
        bool charging = (d[5] & 0x04);

        if (this->low_volt_binary_)     this->low_volt_binary_->publish_state(low_v);
        if (this->weak_battery_binary_) this->weak_battery_binary_->publish_state(weak_b);
        if (this->charging_binary_)     this->charging_binary_->publish_state(charging);
    }
}

} // namespace bm6_ble
} // namespace esphome
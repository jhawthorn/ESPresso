#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_log.h"

#include "espresso.h"

int upgrade_firmware_ota(const char *url) {
    esp_http_client_config_t config = {
        .url = url
    };
    ESP_LOGI("UPGRADE", "Starting firmware upgrade");
    ESP_LOGI("UPGRADE", "url: %s", url);
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        ESP_LOGI("UPGRADE", "Upgrade completed. Restarting.");
        esp_restart();
    } else {
        ESP_LOGW("UPGRADE", "Upgrade failed.");
        return ESP_FAIL;
    }
    return ESP_OK;
}

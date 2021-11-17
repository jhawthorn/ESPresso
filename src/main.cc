#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#include "nvs_flash.h"
#include "Max31865.h"

#include "espresso.h"

#define MAX31865_MISO 12
#define MAX31865_MOSI 13
#define MAX31865_SCK  14
#define MAX31865_CS   15

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

static Max31865 tempSensor(
        MAX31865_MISO,
        MAX31865_MOSI,
        MAX31865_SCK,
        MAX31865_CS);
max31865_config_t tempConfig = {};
max31865_rtd_config_t rtdConfig = {};

void temperature_init()
{
    ESP_LOGI("Temperature", "Initializing");

    tempConfig.vbias = true;
    tempConfig.filter = Max31865Filter::Hz60;
    tempConfig.nWires = Max31865NWires::Three;
    rtdConfig.nominal = 100.0f;
    rtdConfig.ref = 430.0f;

    ESP_ERROR_CHECK(tempSensor.begin(tempConfig));
    ESP_ERROR_CHECK(tempSensor.setRTDThresholds(0x2000, 0x2500));
}

void temperature_loop()
{
    ESP_LOGI("Temperature", "Starting loop");
    while (true) {
        uint16_t rtd;
        Max31865Error fault = Max31865Error::NoError;
        ESP_ERROR_CHECK(tempSensor.getRTD(&rtd, &fault));
        float temp = Max31865::RTDtoTemperature(rtd, rtdConfig);
        ESP_LOGI("Temperature", "%.2f C", temp);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

extern "C" void app_main()
{
    ESP_LOGI("Boot", "Starting");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_sta();

    temperature_init();
}

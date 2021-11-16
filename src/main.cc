#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "Max31865.h"

extern "C" void app_main()
{
    ESP_LOGI("Boot", "Starting");
    auto tempSensor = Max31865(12, 13, 14, 15);
    max31865_config_t tempConfig = {};
    tempConfig.vbias = true;
    tempConfig.filter = Max31865Filter::Hz60;
    tempConfig.nWires = Max31865NWires::Three;
    max31865_rtd_config_t rtdConfig = {};
    rtdConfig.nominal = 100.0f;
    rtdConfig.ref = 430.0f;

    ESP_LOGI("Temperature", "Initializing");
    ESP_ERROR_CHECK(tempSensor.begin(tempConfig));
    ESP_ERROR_CHECK(tempSensor.setRTDThresholds(0x2000, 0x2500));

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

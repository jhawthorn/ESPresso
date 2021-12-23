#include <stdio.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "sdkconfig.h"

#include "esp_event.h"
#include "esp_netif.h"
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

// Used as threshold to determine RTD error state
// Machine will not work if freezing
#define MIN_TEMP (0)
// Safety thermostat trips at 165
#define MAX_TEMP (165)


static Max31865 tempSensor(
        MAX31865_MISO,
        MAX31865_MOSI,
        MAX31865_SCK,
        MAX31865_CS);
max31865_config_t tempConfig = {};
max31865_rtd_config_t rtdConfig = {};

float target_temperature = 94;

void temperature_init()
{
    ESP_LOGI("Temperature", "Initializing");

    tempConfig.vbias = true;
    tempConfig.filter = Max31865Filter::Hz60;
    tempConfig.nWires = Max31865NWires::Two;
    rtdConfig.nominal = 100.0f;
    rtdConfig.ref = 430.0f;

    mqtt_publishf("target", target_temperature);

    ESP_ERROR_CHECK(tempSensor.begin(tempConfig));
}

void temperature_target_set(float val) {
    ESP_LOGI("Temperature", "Setting target: %f", val);
    target_temperature = val;
}

#define PIN_HEAT GPIO_NUM_16
#define PIN_HEAT_ON 1
#define PIN_HEAT_OFF (!PIN_HEAT_ON)

void heating_state_set(bool on) {
    if (on) {
        mqtt_publishs("state", "on");
        ESP_LOGI("Boiler", "on");

	gpio_set_direction(PIN_HEAT, GPIO_MODE_OUTPUT);
	gpio_set_level(PIN_HEAT, PIN_HEAT_ON);
    } else {
        mqtt_publishs("state", "off");
        ESP_LOGI("Boiler", "off");

	//gpio_set_level(PIN_HEAT, PIN_HEAT_OFF);
	gpio_set_direction(PIN_HEAT, GPIO_MODE_DISABLE);
    }
}

void temperature_loop()
{
    ESP_LOGI("Temperature", "Starting loop");
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true) {
        uint16_t rtd;
        Max31865Error fault = Max31865Error::NoError;
        ESP_ERROR_CHECK(tempSensor.getRTD(&rtd, &fault));
        float temp = Max31865::RTDtoTemperature(rtd, rtdConfig);

        if (fault != Max31865Error::NoError) {
            ESP_LOGE("Temperature", "Status: %s", Max31865::errorToString(fault));
            heating_state_set(false);
        } else if (temp < MIN_TEMP || temp > MAX_TEMP) {
            ESP_LOGE("Temperature", "Outside of threshold: %f", temp);
            heating_state_set(false);
        } else {
            ESP_LOGI("Temperature", "%.2f C", temp);
            mqtt_publishf("temperature", temp);
            heating_state_set(temp < target_temperature);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main()
{
    ESP_LOGI("BOOT", "Startup..");
    ESP_LOGI("BOOT", "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI("BOOT", "IDF version: %s", esp_get_idf_version());

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
    mqtt_init();

    temperature_init();

    temperature_loop();
}

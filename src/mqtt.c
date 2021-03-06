#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "espresso.h"

static const char *TAG = "MQTT";

static const char *TOPIC_TARGET = "target";
static const char *TOPIC_UPDATE = "update_url";

static esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_subscribe(esp_mqtt_client_handle_t client, const char *topic) {
    char full_topic[128];
    snprintf(full_topic, 128, CONFIG_MQTT_TOPIC_FORMAT, topic);

    int msg_id = esp_mqtt_client_subscribe(client, full_topic, 1);
    ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
}

static int event_matches(esp_mqtt_event_handle_t event, const char *topic) {
    char full_topic[128];

    int len = snprintf(full_topic, 128, CONFIG_MQTT_TOPIC_FORMAT, topic);
    if (len <= 0) {
        ESP_LOGE(TAG, "snprintf error");
        return 0;
    }

    return event->topic_len == len && !memcmp(event->topic, full_topic, len);
}

static bool parse_float(const char *data, int data_len, float *ret) {
    char buf[32];
    if (data_len >= sizeof(buf)) {
        ESP_LOGW(TAG, "received data too long for float");
        return 0;
    }
    memcpy(buf, data, data_len);
    buf[data_len] = '\0';

    *ret = atof(buf);

    return 1;
}

static void mqtt_receive_data(esp_mqtt_event_handle_t event) {

    if (event_matches(event, TOPIC_TARGET)) {
        float f_data = 0;
        if (!parse_float(event->data, event->data_len, &f_data))
            return;

        temperature_target_set(f_data);
    } else if (event_matches(event, TOPIC_UPDATE)) {
        char buf[128];
        if (event->data_len >= sizeof(buf)) {
            ESP_LOGW(TAG, "received update URL too long");
            return;
        }
        memcpy(buf, event->data, event->data_len);
        buf[event->data_len] = 0;

        upgrade_firmware_ota(buf);
    } else {
        ESP_LOGW(TAG, "unknown topic");
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        mqtt_subscribe(client, TOPIC_TARGET);
        mqtt_subscribe(client, TOPIC_UPDATE);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");

        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        mqtt_receive_data(event);

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_MQTT_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_publishs(const char *topic, const char *value) {
    char full_topic[128];
    snprintf(full_topic, 128, CONFIG_MQTT_TOPIC_FORMAT, topic);

    int qos = 0;
    int retain = 0;
    esp_mqtt_client_publish(client, full_topic, value, 0, qos, retain);
}

void mqtt_publishf(const char *topic, float value) {
    char formatted_value[128];
    snprintf(formatted_value, 128, "%.2f", value);
    mqtt_publishs(topic, formatted_value);
}

#include "mqtt.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <mqtt_client.h>

#include "data.h"
#include "events.h"

#define TAG "mqtt"

#define CONFIG_BROKER_URL "mqtt://10.32.0.18"
#define MAX_TOPIC_SIZE (64)

typedef struct
{
    char duty[MAX_TOPIC_SIZE];
    char status[MAX_TOPIC_SIZE];
} mqtt_topics_t;

static esp_mqtt_client_handle_t m_client;
static mqtt_topics_t m_topics;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    ESP_LOGD(TAG, "free heap size is %" PRIu32 ", minimum %" PRIu32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, m_topics.duty, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_DATA:
        if (strcmp(event->topic, m_topics.duty))
        {
            data_process_duty_json_str(event->data, event->data_len);
        }
        else
        {
            ESP_LOGW(TAG, "MQTT_EVENT_DATA %.*s (not matched); %.*s", event->topic_len, event->topic, event->data_len, event->data);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        ESP_LOGI(TAG, "MQTT5 return code is %d", event->error_handle->connect_return_code);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    case MQTT_EVENT_SUBSCRIBED:
    case MQTT_EVENT_PUBLISHED:
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void status_handler(void *_event_handler_arg,
                           esp_event_base_t _event_base,
                           int32_t _event_id,
                           void *_event_data)
{
    cJSON *root = cJSON_CreateObject();
    ESP_ERROR_CHECK(data_status_to_json(root));
    const char *resp = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(m_client, m_topics.status, resp, 0, 0, 0);
    free((void *)resp);

    cJSON_Delete(root);
}

esp_err_t mqtt_init(void)
{
    snprintf(m_topics.duty, MAX_TOPIC_SIZE, "fancontroller/%s/duty", data_get_id());
    snprintf(m_topics.status, MAX_TOPIC_SIZE, "fancontroller/%s/status", data_get_id());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .session.protocol_ver = MQTT_PROTOCOL_UNDEFINED,
        .network.disable_auto_reconnect = false,
        .network.reconnect_timeout_ms = 1000,
    };

    m_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(m_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(m_client);

    esp_event_handler_instance_register(EVENTS, EVENT_STATUS_PING,
                                        status_handler, NULL, NULL);

    return ESP_OK;
}

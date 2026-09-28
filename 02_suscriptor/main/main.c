/*
 * Práctica: Protocolo ligero para IoT MQTT - Suscriptor
 * Plataforma: ESP32-C6
 * Framework: ESP-IDF 6.x
 *
 * Función:
 * - Se conecta a Wi-Fi usando protocol_examples_common.
 * - Se conecta al broker MQTT de ThingSpeak.
 * - Se suscribe a channels/<CHANNEL_ID>/subscribe.
 * - Imprime por consola el tópico y el payload recibido.
 *
 * Configuración:
 * 1) idf.py menuconfig
 * 2) Example Connection Configuration -> SSID / Password
 * 3) ThingSpeak MQTT Configuration -> credenciales MQTT y Channel ID
 */

#include <stdio.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "mqtt_client.h"

static const char *TAG = "MQTT_SUSCRIPTOR";
static const char *BROKER_URI = "mqtt://mqtt3.thingspeak.com:1883";

static esp_mqtt_client_handle_t mqtt_client;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event =
        (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            char topic[96];

            snprintf(topic,
                     sizeof(topic),
                     "channels/%s/subscribe",
                     CONFIG_THINGSPEAK_CHANNEL_ID);

            int msg_id = esp_mqtt_client_subscribe(mqtt_client,
                                                   topic,
                                                   0);

            ESP_LOGI(TAG,
                     "Conectado. Solicitud de suscripcion enviada, msg_id=%d",
                     msg_id);
            break;
        }

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG,
                     "Suscripcion confirmada. Esperando actualizaciones...");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Dato recibido desde ThingSpeak");
            printf("Topico: %.*s\n",
                   event->topic_len,
                   event->topic);
            printf("Payload: %.*s\n",
                   event->data_len,
                   event->data);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado del broker MQTT");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Se produjo un error MQTT");
            break;

        default:
            break;
    }
}

static void inicializar_nvs(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando suscriptor MQTT con ESP32-C6");

    inicializar_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = BROKER_URI,
        .credentials.client_id = CONFIG_THINGSPEAK_CLIENT_ID,
        .credentials.username = CONFIG_THINGSPEAK_USERNAME,
        .credentials.authentication.password =
            CONFIG_THINGSPEAK_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el cliente MQTT");
        return;
    }

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(mqtt_client,
                                       ESP_EVENT_ANY_ID,
                                       mqtt_event_handler,
                                       NULL)
    );

    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

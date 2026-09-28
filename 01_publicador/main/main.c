/*
 * Práctica: Protocolo ligero para IoT MQTT - Publicador
 * Plataforma: ESP32-C6
 * Framework: ESP-IDF 6.x
 *
 * Función:
 * - Se conecta a Wi-Fi usando protocol_examples_common.
 * - Lee el sensor interno de temperatura del ESP32-C6.
 * - Publica la temperatura en field1 de un canal ThingSpeak mediante MQTT.
 *
 * Configuración:
 * 1) idf.py menuconfig
 * 2) Example Connection Configuration -> SSID / Password
 * 3) ThingSpeak MQTT Configuration -> credenciales MQTT y Channel ID
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "driver/temperature_sensor.h"
#include "mqtt_client.h"

#define MQTT_CONNECTED_BIT BIT0
#define PUBLISH_PERIOD_MS 20000

static const char *TAG = "MQTT_PUBLICADOR";
static const char *BROKER_URI = "mqtt://mqtt3.thingspeak.com:1883";

static EventGroupHandle_t mqtt_event_group;
static esp_mqtt_client_handle_t mqtt_client;
static temperature_sensor_handle_t temp_sensor = NULL;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al broker MQTT de ThingSpeak");
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado del broker MQTT");
            xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Se produjo un error MQTT");
            break;

        default:
            break;
    }
}

static void tarea_publicacion(void *pv_parameters)
{
    (void)pv_parameters;

    char topic[96];
    char payload[96];

    snprintf(topic,
             sizeof(topic),
             "channels/%s/publish",
             CONFIG_THINGSPEAK_CHANNEL_ID);

    while (1) {
        xEventGroupWaitBits(mqtt_event_group,
                            MQTT_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        float temperatura_c = 0.0f;
        esp_err_t err = temperature_sensor_get_celsius(temp_sensor,
                                                       &temperatura_c);

        if (err == ESP_OK) {
            snprintf(payload,
                     sizeof(payload),
                     "field1=%.2f&status=ESP32C6_MQTT",
                     temperatura_c);

            int msg_id = esp_mqtt_client_publish(mqtt_client,
                                                 topic,
                                                 payload,
                                                 0,
                                                 0,
                                                 0);

            ESP_LOGI(TAG,
                     "Temperatura: %.2f C | Topic: %s | msg_id: %d",
                     temperatura_c,
                     topic,
                     msg_id);
        } else {
            ESP_LOGE(TAG,
                     "No fue posible leer el sensor de temperatura: %s",
                     esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(PUBLISH_PERIOD_MS));
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

static void inicializar_sensor_temperatura(void)
{
    temperature_sensor_config_t config =
        TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    ESP_ERROR_CHECK(
        temperature_sensor_install(&config, &temp_sensor)
    );

    ESP_ERROR_CHECK(
        temperature_sensor_enable(temp_sensor)
    );
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando publicador MQTT con ESP32-C6");

    inicializar_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    inicializar_sensor_temperatura();

    mqtt_event_group = xEventGroupCreate();
    if (mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el Event Group");
        return;
    }

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

    xTaskCreate(tarea_publicacion,
                "tarea_publicacion",
                4096,
                NULL,
                5,
                NULL);
}

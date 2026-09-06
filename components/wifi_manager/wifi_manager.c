#include "wifi_manager.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_event_group;
static int                s_retry_count;
static bool               s_connected;

/* --------------------------------------------------------------------------
 * Event handler
 * -------------------------------------------------------------------------- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA start, connecting...");
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Initial connect failed: %s", esp_err_to_name(err));
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;

        if (s_retry_count < CONFIG_APP_WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "Disconnected; retry %d/%d",
                     s_retry_count, CONFIG_APP_WIFI_MAX_RETRIES);

            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Connect failed: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "Max retries reached");
            xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event =
            (const ip_event_got_ip_t *)event_data;

        s_connected = true;
        s_retry_count = 0;

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

esp_err_t wifi_manager_init(void)
{
    s_connected   = false;
    s_retry_count = 0;

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    s_retry_count = 0;

    wifi_config_t wifi_config = {0};

    size_t ssid_len = strlen(CONFIG_APP_WIFI_SSID);
    if (ssid_len > sizeof(wifi_config.sta.ssid) - 1) {
        ssid_len = sizeof(wifi_config.sta.ssid) - 1;
    }
    memcpy(wifi_config.sta.ssid, CONFIG_APP_WIFI_SSID, ssid_len);

    size_t pass_len = strlen(CONFIG_APP_WIFI_PASSWORD);
    if (pass_len > sizeof(wifi_config.sta.password) - 1) {
        pass_len = sizeof(wifi_config.sta.password) - 1;
    }
    memcpy(wifi_config.sta.password, CONFIG_APP_WIFI_PASSWORD, pass_len);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

wifi_result_t wifi_manager_wait_for_connection(uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        s_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        ticks);

    if (bits & WIFI_CONNECTED_BIT) {
        return WIFI_RESULT_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        return WIFI_RESULT_FAILED;
    }

    return WIFI_RESULT_TIMEOUT;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_manager_stop(void)
{
    s_connected = false;
    return esp_wifi_stop();
}

esp_err_t wifi_manager_deinit(void)
{
    s_connected = false;
    esp_err_t err = esp_wifi_deinit();

    if (s_event_group != NULL) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
    }

    return err;
}

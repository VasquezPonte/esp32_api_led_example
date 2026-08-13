#include "sntp_manager.h"

#include <time.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "sntp_mgr";

esp_err_t sntp_manager_init(void)
{
    /* Placeholder for any pre-sync initialization */
    return ESP_OK;
}

esp_err_t sntp_manager_sync_time(uint32_t timeout_ms, int max_retries)
{
    ESP_LOGI(TAG, "Starting SNTP sync with %s", CONFIG_APP_SNTP_SERVER);

    esp_sntp_config_t config =
        ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_APP_SNTP_SERVER);

    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        return err;
    }

    setenv("TZ", CONFIG_APP_TIMEZONE, 1);
    tzset();

    TickType_t retry_delay = pdMS_TO_TICKS(timeout_ms);
    if (retry_delay == 0) {
        retry_delay = portMAX_DELAY;
    }

    for (int retry = 0; retry < max_retries; retry++) {
        err = esp_netif_sntp_sync_wait(retry_delay);

        if (err == ESP_OK) {
            time_t now;
            struct tm timeinfo;

            time(&now);
            localtime_r(&now, &timeinfo);

            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);

            ESP_LOGI(TAG, "Time synchronized: %s", buf);
            esp_netif_sntp_deinit();
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Sync attempt %d/%d failed", retry + 1, max_retries);
    }

    ESP_LOGE(TAG, "SNTP synchronization failed after %d attempts", max_retries);
    esp_netif_sntp_deinit();
    return ESP_ERR_TIMEOUT;
}

esp_err_t sntp_manager_deinit(void)
{
    return ESP_OK;
}

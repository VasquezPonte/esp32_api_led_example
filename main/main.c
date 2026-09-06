/*
 * ESP32 API LED Example
 *
 * ESP-IDF: 6.0.2
 * 
 * Application State Machine — Infinite Loop
 *
 * States:
 *   INIT          -> NVS init
 *   WIFI_CONNECT  -> Start Wi-Fi and wait for IP
 *   TIME_SYNC     -> SNTP sync (once)
 *   API_REQUEST   -> HTTPS GET
 *   PROCESS_RESULT-> Parse JSON, drive LED
 *   DELAY         -> Wait APP_LOOP_INTERVAL_MS, then loop back to API_REQUEST
 *
 * Transitions on error:
 *   Any failure in API_REQUEST / WIFI_CONNECT -> DELAY (retry after interval)
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "sntp_manager.h"
#include "https_client.h"

static const char *TAG = "app";

/* --------------------------------------------------------------------------
 * LED helpers
 * -------------------------------------------------------------------------- */

static inline void led_init(void)
{
    gpio_reset_pin(CONFIG_APP_LED_GPIO_PIN);
    gpio_set_direction(CONFIG_APP_LED_GPIO_PIN, GPIO_MODE_OUTPUT);

#ifdef CONFIG_APP_LED_ACTIVE_LOW
    gpio_set_level(CONFIG_APP_LED_GPIO_PIN, 1); /* LED off */
#else
    gpio_set_level(CONFIG_APP_LED_GPIO_PIN, 0); /* LED off */
#endif
}

static inline void led_set(bool on)
{
#ifdef CONFIG_APP_LED_ACTIVE_LOW
    gpio_set_level(CONFIG_APP_LED_GPIO_PIN, on ? 0 : 1);
#else
    gpio_set_level(CONFIG_APP_LED_GPIO_PIN, on ? 1 : 0);
#endif
}

/* --------------------------------------------------------------------------
 * State-machine definitions
 * -------------------------------------------------------------------------- */

typedef enum {
    STATE_INIT = 0,
    STATE_WIFI_CONNECT,
    STATE_TIME_SYNC,
    STATE_API_REQUEST,
    STATE_PROCESS_RESULT,
    STATE_DELAY,
    STATE_SHUTDOWN
} app_state_t;

typedef struct {
    app_state_t state;
    int         api_value;
    bool        time_synced;
    uint32_t    error_count;
} app_context_t;

/* --------------------------------------------------------------------------
 * State handlers
 * -------------------------------------------------------------------------- */

static app_state_t state_init(app_context_t *ctx)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "STATE: INIT");
    ESP_LOGI(TAG, "========================================");

    led_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_LOGW(TAG, "NVS needs erase");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    return STATE_WIFI_CONNECT;
}

static app_state_t state_wifi_connect(app_context_t *ctx)
{
    ESP_LOGI(TAG, "STATE: WIFI_CONNECT");

    esp_err_t err = wifi_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        ctx->error_count++;
        return STATE_DELAY;
    }

    err = wifi_manager_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(err));
        wifi_manager_deinit();
        ctx->error_count++;
        return STATE_DELAY;
    }

    wifi_result_t result = wifi_manager_wait_for_connection(30000); /* 30 s timeout */

    if (result != WIFI_RESULT_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed (result=%d)", result);
        wifi_manager_stop();
        wifi_manager_deinit();
        ctx->error_count++;
        return STATE_DELAY;
    }

    ESP_LOGI(TAG, "Wi-Fi connected");
    return ctx->time_synced ? STATE_API_REQUEST : STATE_TIME_SYNC;
}

static app_state_t state_time_sync(app_context_t *ctx)
{
    ESP_LOGI(TAG, "STATE: TIME_SYNC");

    esp_err_t err = sntp_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        ctx->error_count++;
        return STATE_DELAY;
    }

    err = sntp_manager_sync_time(CONFIG_APP_SNTP_RETRY_DELAY_MS,
                                 CONFIG_APP_SNTP_MAX_RETRIES);

    sntp_manager_deinit();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP sync failed: %s", esp_err_to_name(err));
        ctx->error_count++;
        return STATE_DELAY;
    }

    ESP_LOGI(TAG, "System time synchronized");
    ctx->time_synced = true;
    return STATE_API_REQUEST;
}

static app_state_t state_api_request(app_context_t *ctx)
{
    ESP_LOGI(TAG, "STATE: API_REQUEST");

    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "Wi-Fi not connected, reconnecting...");
        wifi_manager_stop();
        wifi_manager_deinit();
        return STATE_WIFI_CONNECT;
    }

    esp_err_t err = https_client_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS client init failed: %s", esp_err_to_name(err));
        ctx->error_count++;
        return STATE_DELAY;
    }

    err = https_client_fetch_int_value(&ctx->api_value);
    https_client_deinit();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "API request failed: %s", esp_err_to_name(err));
        ctx->error_count++;
        return STATE_DELAY;
    }

    return STATE_PROCESS_RESULT;
}

static app_state_t state_process_result(app_context_t *ctx)
{
    ESP_LOGI(TAG, "STATE: PROCESS_RESULT | value=%d | threshold=%d",
             ctx->api_value, CONFIG_APP_LED_THRESHOLD);

    if (ctx->api_value < CONFIG_APP_LED_THRESHOLD) {
        ESP_LOGI(TAG, "Value < %d -> LED OFF", CONFIG_APP_LED_THRESHOLD);
        led_set(false);
    } else {
        ESP_LOGI(TAG, "Value >= %d -> LED ON", CONFIG_APP_LED_THRESHOLD);
        led_set(true);
    }

    ctx->error_count = 0; /* Reset error streak on success */
    return STATE_DELAY;
}

static app_state_t state_delay(app_context_t *ctx)
{
    ESP_LOGI(TAG, "STATE: DELAY (%d ms)", CONFIG_APP_LOOP_INTERVAL_MS);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_LOOP_INTERVAL_MS));

    /* After delay, always try the next API request.
     * If Wi-Fi dropped, state_api_request will detect it and reroute. */
    return STATE_API_REQUEST;
}

static app_state_t state_shutdown(app_context_t *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "STATE: SHUTDOWN");
    led_set(false);
    wifi_manager_stop();
    wifi_manager_deinit();
    return STATE_SHUTDOWN;
}

/* --------------------------------------------------------------------------
 * Main entry point
 * -------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting API LED Example (Infinite Loop)");

    app_context_t ctx = {
        .state       = STATE_INIT,
        .api_value   = 0,
        .time_synced = false,
        .error_count = 0,
    };

    app_state_t next = STATE_INIT;

    while (next != STATE_SHUTDOWN) {
        ctx.state = next;

        switch (ctx.state) {
        case STATE_INIT:
            next = state_init(&ctx);
            break;

        case STATE_WIFI_CONNECT:
            next = state_wifi_connect(&ctx);
            break;

        case STATE_TIME_SYNC:
            next = state_time_sync(&ctx);
            break;

        case STATE_API_REQUEST:
            next = state_api_request(&ctx);
            break;

        case STATE_PROCESS_RESULT:
            next = state_process_result(&ctx);
            break;

        case STATE_DELAY:
            next = state_delay(&ctx);
            break;

        default:
            next = state_shutdown(&ctx);
            break;
        }
    }

    ESP_LOGI(TAG, "State machine terminated");
}

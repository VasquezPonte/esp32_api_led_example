#include "https_client.h"

#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "https_client";

static char   s_resp_buffer[CONFIG_APP_RESPONSE_BUFFER_SIZE];
static size_t s_resp_len;

/* --------------------------------------------------------------------------
 * HTTP event handler
 * -------------------------------------------------------------------------- */

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {

    case HTTP_EVENT_ON_DATA:
        if (evt->data_len <= 0) {
            break;
        }

        if (s_resp_len + (size_t)evt->data_len >= sizeof(s_resp_buffer)) {
            ESP_LOGE(TAG, "Response exceeds buffer (%d bytes)",
                     CONFIG_APP_RESPONSE_BUFFER_SIZE);
            return ESP_ERR_NO_MEM;
        }

        memcpy(s_resp_buffer + s_resp_len, evt->data, evt->data_len);
        s_resp_len += (size_t)evt->data_len;
        s_resp_buffer[s_resp_len] = '\0';
        break;

    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "Transfer finished, %d bytes received", (int)s_resp_len);
        break;

    default:
        break;
    }

    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

esp_err_t https_client_init(void)
{
    /* Placeholder for future persistent client handles, headers, etc. */
    return ESP_OK;
}

esp_err_t https_client_fetch_int_value(int *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_resp_len = 0;
    memset(s_resp_buffer, 0, sizeof(s_resp_buffer));

    esp_http_client_config_t cfg = {
        .url               = CONFIG_APP_API_URL,
        .method            = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler     = http_event_handler,
        .timeout_ms        = CONFIG_APP_HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
        .tls_version       = ESP_HTTP_CLIENT_TLS_VER_TLS_1_3,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GET %s", CONFIG_APP_API_URL);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    const int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP status: %d", status_code);
    ESP_LOGD(TAG, "Response body: %s", s_resp_buffer);

    esp_http_client_cleanup(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "Server returned HTTP status %d", status_code);
        return ESP_FAIL;
    }

    /* ----------------------------------------------------------------------
     * Parse JSON
     * ---------------------------------------------------------------------- */

    cJSON *root = cJSON_Parse(s_resp_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON response");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *field = cJSON_GetObjectItemCaseSensitive(
        root, CONFIG_APP_API_JSON_FIELD);

    if (!cJSON_IsNumber(field)) {
        ESP_LOGE(TAG, "JSON field '%s' missing or not a number",
                 CONFIG_APP_API_JSON_FIELD);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *value = field->valueint;

    ESP_LOGI(TAG, "Parsed value: %d", *value);

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t https_client_deinit(void)
{
    return ESP_OK;
}

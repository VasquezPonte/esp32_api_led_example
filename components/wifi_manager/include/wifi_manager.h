#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result codes for Wi-Fi connection attempts
 */
typedef enum {
    WIFI_RESULT_OK,          /**< Connected and IP obtained */
    WIFI_RESULT_TIMEOUT,     /**< Timed out waiting for connection */
    WIFI_RESULT_FAILED,      /**< Exhausted all retries */
    WIFI_RESULT_INVALID_ARG, /**< Invalid argument */
    WIFI_RESULT_NO_MEM,      /**< Memory allocation failed */
} wifi_result_t;

/**
 * @brief Initialize Wi-Fi driver, netif, and event handlers
 *
 * @return esp_err_t
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start Wi-Fi STA and begin connection attempt
 *
 * @return esp_err_t
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Block until connected, failed, or timed out
 *
 * @param timeout_ms Maximum time to wait in milliseconds.
 *                   Use portMAX_DELAY to block indefinitely.
 * @return wifi_result_t
 */
wifi_result_t wifi_manager_wait_for_connection(uint32_t timeout_ms);

/**
 * @brief Disconnect and stop Wi-Fi
 *
 * @return esp_err_t
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Deinitialize Wi-Fi and free resources
 *
 * @return esp_err_t
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Query whether the STA currently has an IP address
 * @return true if connected, false otherwise
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif

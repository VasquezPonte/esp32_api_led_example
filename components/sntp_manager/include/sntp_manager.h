#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SNTP manager (no-op placeholder for future expansion)
 */
esp_err_t sntp_manager_init(void);

/**
 * @brief Synchronize system time with the configured NTP server
 *
 * @param timeout_ms  Timeout per attempt (milliseconds)
 * @param max_retries Maximum number of sync attempts
 * @return esp_err_t  ESP_OK on success, ESP_ERR_TIMEOUT on exhaustion
 */
esp_err_t sntp_manager_sync_time(uint32_t timeout_ms, int max_retries);

/**
 * @brief Deinitialize SNTP manager
 */
esp_err_t sntp_manager_deinit(void);

#ifdef __cplusplus
}
#endif

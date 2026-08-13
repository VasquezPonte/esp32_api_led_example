#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize HTTPS client component
 */
esp_err_t https_client_init(void);

/**
 * @brief Perform HTTPS GET and extract the configured integer JSON field
 *
 * @param[out] value Pointer to store the parsed integer
 * @return esp_err_t
 */
esp_err_t https_client_fetch_int_value(int *value);

/**
 * @brief Deinitialize HTTPS client component
 */
esp_err_t https_client_deinit(void);

#ifdef __cplusplus
}
#endif

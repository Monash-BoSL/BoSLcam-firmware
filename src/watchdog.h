/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**@file
 *
 * @brief   Watchdog module for Asset Tracker v2
 */

#ifndef WATCHDOG_H__
#define WATCHDOG_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

enum watchdog_evt_type {
	WATCHDOG_EVT_START,
	WATCHDOG_EVT_TIMEOUT_INSTALLED,
	WATCHDOG_EVT_FEED
};

struct watchdog_evt {
	enum watchdog_evt_type type;
	uint32_t timeout;
};

/** @brief Watchdog library event handler.
 *
 *  @param[in] evt The event and any associated parameters.
 */
typedef void (*watchdog_evt_handler_t)(const struct watchdog_evt *evt);

/** @brief Initialize and start application watchdog module.
 *
 *  @return Zero on success, otherwise a negative error code is returned.
 */
int watchdog_init_and_start(void);

/** @brief Feed the watchdog timer
 *
 */
void watchdog_feed();


#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_APP_H__ */

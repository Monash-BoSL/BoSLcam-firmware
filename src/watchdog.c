/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "watchdog.h"
#include "common.h"

LOG_MODULE_REGISTER(watchdog);

#define WATCHDOG_TIMEOUT_MSEC						\
	(WATCHDOG_TIMEOUT_SEC * 1000)

struct wdt_data_storage {
	const struct device *wdt_drv;
	int wdt_channel_id;
	struct k_work_delayable system_workqueue_work;
};

/* Flag set when the library has been initialized and started. */
static struct wdt_data_storage wdt_data;
static int watchdog_start(struct wdt_data_storage *data);

static int watchdog_timeout_install(struct wdt_data_storage *data)
{
	static const struct wdt_timeout_cfg wdt_settings = {
		.window = {
			.min = 0,
			.max = WATCHDOG_TIMEOUT_MSEC,
		},
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC
	};
	__ASSERT_NO_MSG(data != NULL);

	data->wdt_channel_id =
		wdt_install_timeout(data->wdt_drv, &wdt_settings);
	if (data->wdt_channel_id < 0) {
		LOG_ERR("Cannot install watchdog timer! Error code: %d",
			data->wdt_channel_id);
		return -EFAULT;
	}

	LOG_INF("Watchdog timeout installed. Timeout: %d",
		WATCHDOG_TIMEOUT_SEC);
	return 0;
}

int watchdog_start(struct wdt_data_storage *data)
{
	__ASSERT_NO_MSG(data != NULL);

	int err = wdt_setup(data->wdt_drv, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (err) {
		LOG_ERR("Cannot start watchdog! Error code: %d", err);
	} else {
		LOG_INF("Watchdog started");
	}
	return err;
}

static int watchdog_enable(struct wdt_data_storage *data)
{
	__ASSERT_NO_MSG(data != NULL);

	int err = -ENXIO;

	data->wdt_drv = DEVICE_DT_GET(DT_NODELABEL(wdt));
	if (data->wdt_drv == NULL) {
		LOG_ERR("Cannot bind watchdog driver");
		return err;
	}

	err = watchdog_timeout_install(data);
	if (err) {
		return err;
	}

	err = watchdog_start(data);
	if (err) {
		return err;
	}

	return 0;
}

int watchdog_init_and_start(void)
{
	int err = watchdog_enable(&wdt_data);
	if (err) {
		LOG_ERR("Failed to enable watchdog, error: %d", err);
		return err;
	}else{
		LOG_INF("Enabled watchdog success!");	
	}

	return 0;
}

void watchdog_feed()
{

	wdt_feed(wdt_data.wdt_drv, wdt_data.wdt_channel_id);

	LOG_INF("fed");

}
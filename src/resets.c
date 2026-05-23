
#include "common.h"


LOG_MODULE_REGISTER(resets);

void log_reset_reason(void) {
    uint32_t cause = 0;

    int err = hwinfo_get_reset_cause(&cause);
    if (err != 0) {
        LOG_ERR("hwinfo_get_reset_cause failed: %d", err);
        return;
    }

    LOG_INF("Reset cause: 0x%08x", cause);

    if (cause & RESET_PIN) {
        LOG_INF("Pin reset");
    }

    if (cause & RESET_WATCHDOG) {
        LOG_INF("Watchdog reset");
    }

    if (cause & RESET_CPU_LOCKUP) {
        LOG_INF("CPU lockup");
    }

    if (cause & RESET_SOFTWARE) {
        LOG_INF("Software reset");
    }

    if (cause == 0) {
        LOG_INF("POR/BOR reset");
    }

    hwinfo_clear_reset_cause();
}
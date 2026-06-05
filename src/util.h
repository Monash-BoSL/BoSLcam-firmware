#pragma once

#include <inttypes.h>
#include <time.h>

#include "common.h"

void unix_date(struct tm* cal, int32_t unixtime);

int strfstatus(char* buf, const size_t len, const struct status_t* status, const struct capture_task_t* capture_task);
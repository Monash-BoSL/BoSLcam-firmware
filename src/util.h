#pragma once

#include <inttypes.h>
#include <time.h>


int encrypt(char* msg, int key);
int decrypt(char* msg, int key);

void unix_date(struct tm* cal, int32_t unixtime);
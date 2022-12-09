#pragma once

static int lsdir(const char* path);
void sdhc_info(void);
int sdhc_write_image(char* sdhc_path, char* data, uint32_t length);
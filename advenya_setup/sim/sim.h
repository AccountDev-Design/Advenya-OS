#pragma once
#include <stddef.h>
#include <stdint.h>
#include "advenya_settings.h"
uint32_t sim_now_ms(void);
size_t sim_heap_in_use(void);
const adv_settings_backend_t *sim_settings_backend(const char *path);

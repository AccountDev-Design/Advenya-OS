#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    volatile uint32_t frames;
    uint64_t render_sum_us;
    uint32_t render_max_us;
    uint32_t flush_wait_max_us;
    uint32_t flush_timeouts;
    uint32_t flush_errors;
    bool     in_frame;
    int64_t  frame_t0_us;
} adv_port_stats_t;

extern adv_port_stats_t g_adv_port_stats;

TaskHandle_t adv_lvgl_task_handle(void);
void adv_diag_start(void);

/* ADVENYA setup · servicios de plataforma del SIMULADOR (todo sincrono). */
#include <stdio.h>
#include <stdlib.h>
#include "ui_platform.h"
#include "sim.h"

void *ui_platform_alloc_large(size_t bytes)
{
    return malloc(bytes);
}

void ui_platform_free_large(void *p)
{
    free(p);
}

bool ui_platform_run_async(void (*work)(void *), void (*done)(void *), void *arg)
{
    (void)work;
    (void)done;
    (void)arg;
    return false;          /* el llamador lo ejecuta en linea */
}

int ui_platform_is_night(void)
{
    const char *v = getenv("ADV_SIM_NIGHT");
    return v ? atoi(v) : -1;
}

void ui_platform_log_mem(const char *where)
{
    printf("  [mem] %s: %zu B en uso\n", where, sim_heap_in_use());
}

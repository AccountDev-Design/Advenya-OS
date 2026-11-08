/* ADVENYA setup · backend de persistencia del SIMULADOR: un archivo de 16 B. */
#include <stdio.h>
#include "advenya_settings.h"
#include "sim.h"

static const char *s_path;

static bool f_load(adv_settings_t *out)
{
    FILE *f = fopen(s_path, "rb");
    if (!f) {
        return false;
    }
    size_t n = fread(out, 1, sizeof(*out), f);
    fclose(f);
    return n == sizeof(*out);
}

static bool f_save(const adv_settings_t *in)
{
    FILE *f = fopen(s_path, "wb");
    if (!f) {
        return false;
    }
    size_t n = fwrite(in, 1, sizeof(*in), f);
    fclose(f);
    return n == sizeof(*in);
}

static void f_erase(void)
{
    remove(s_path);
}

static uint32_t f_now(void)
{
    return sim_now_ms();
}

const adv_settings_backend_t *sim_settings_backend(const char *path)
{
    static const adv_settings_backend_t be = {f_load, f_save, f_erase, f_now};
    s_path = path;
    return &be;
}

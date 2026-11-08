#!/usr/bin/env python3
"""
Comprueba que los datos de hardware de advenya_board coinciden con la
implementacion de referencia de Flex OS Ultra.

Uso: python3 tools/check_hw_vs_flex.py /ruta/a/Flex-OS-Ecosistema
     (rama claude/flexos-ota-system-blq9g9-2csacx)
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def arrays(src, prefix):
    return {m.group(1): [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(2))]
            for m in re.finditer(r"static const uint8_t (%s\w+)\[\]\s*=\s*\{([^}]*)\}" % prefix, src)}


def main():
    flex_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "..", "..", "Flex-OS-Ecosistema")
    hal = open(os.path.join(flex_dir, "FlexOS_Ultra", "FlexOS_Ultra_HAL.h"), encoding="utf-8").read()
    types = open(os.path.join(flex_dir, "FlexOS_Ultra", "FlexOS_Ultra_Types.h"), encoding="utf-8").read()
    tbl = open(os.path.join(ROOT, "components", "advenya_board", "src", "st7701_init_table.h"), encoding="utf-8").read()
    pins = open(os.path.join(ROOT, "components", "advenya_board", "include", "advenya_board_pins.h"), encoding="utf-8").read()
    ok = True

    fa, aa = arrays(hal, "r"), arrays(tbl, "st_")
    start = hal.find("ST7701_INIT[]")
    frows = re.findall(r"\{(0x[0-9A-F]{2}),(\d+),(\w+)\}", hal[start:hal.find("};", start)])
    arows = re.findall(r"ST_ROW\((0x[0-9A-F]{2}),\s*(\w+)\)", tbl)
    fseq = [(int(c, 16), fa.get(n, [])) for c, _, n in frows]
    aseq = [(int(c, 16), aa[n]) for c, n in arows] + [(0x20, [])]
    same = fseq == aseq
    ok &= same
    print("tabla DCS ST7701: %d filas, %s" % (len(fseq), "IDENTICA" if same else "DIFERENTE"))

    def flex_val(pattern, src=hal):
        m = re.search(pattern, src)
        return int(m.group(1)) if m else None

    def adv_val(name):
        m = re.search(r"#define %s\s+(?:GPIO_NUM_)?(\d+)" % name, pins)
        return int(m.group(1)) if m else None

    checks = [
        ("LDO canal", r"ldo\.chan_id\s*=\s*(\d+)", "ADV_MIPI_PHY_LDO_CHAN"),
        ("LDO mV", r"ldo\.voltage_mv\s*=\s*(\d+)", "ADV_MIPI_PHY_LDO_MV"),
        ("lanes DSI", r"bus\.num_data_lanes\s*=\s*(\d+)", "ADV_DSI_LANES"),
        ("Mbps/lane", r"bus\.lane_bit_rate_mbps\s*=\s*(\d+)", "ADV_DSI_LANE_MBPS"),
        ("DPI MHz", r"dpi\.dpi_clock_freq_mhz\s*=\s*(\d+)", "ADV_DPI_CLOCK_MHZ"),
        ("HSYNC pw", r"hsync_pulse_width\s*=\s*(\d+)", "ADV_DPI_HSYNC_PW"),
        ("HSYNC bp", r"hsync_back_porch\s*=\s*(\d+)", "ADV_DPI_HSYNC_BP"),
        ("HSYNC fp", r"hsync_front_porch\s*=\s*(\d+)", "ADV_DPI_HSYNC_FP"),
        ("VSYNC pw", r"vsync_pulse_width\s*=\s*(\d+)", "ADV_DPI_VSYNC_PW"),
        ("VSYNC bp", r"vsync_back_porch\s*=\s*(\d+)", "ADV_DPI_VSYNC_BP"),
        ("VSYNC fp", r"vsync_front_porch\s*=\s*(\d+)", "ADV_DPI_VSYNC_FP"),
    ]
    for label, pat, name in checks:
        f, a = flex_val(pat), adv_val(name)
        ok &= f == a
        print("%-10s Flex=%s ADVENYA=%s %s" % (label, f, a, "OK" if f == a else "DIFERENTE"))
    for fname, aname in (("PIN_LCD_RST", "ADV_PIN_LCD_RST"), ("PIN_LCD_BL", "ADV_PIN_LCD_BL"),
                         ("PIN_TP_SDA", "ADV_PIN_TP_SDA"), ("PIN_TP_SCL", "ADV_PIN_TP_SCL"),
                         ("PIN_TP_RST", "ADV_PIN_TP_RST"), ("SCR_W", "ADV_LCD_H_RES"), ("SCR_H", "ADV_LCD_V_RES")):
        f = flex_val(r"#define %s\s+(\d+)" % fname, types)
        a = adv_val(aname)
        ok &= f == a
        print("%-10s Flex=%s ADVENYA=%s %s" % (fname, f, a, "OK" if f == a else "DIFERENTE"))
    print("RESULTADO:", "todo coincide" if ok else "HAY DIFERENCIAS")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

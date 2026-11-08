#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ADVENYA setup · bateria de PRUEBAS SIMULADAS (host)

Compila el simulador (sim/CMakeLists.txt) y ejecuta los escenarios en el
orden en que se encadenan los "arranques" de la placa. Cada escenario es un
proceso nuevo: terminar el proceso equivale a cortar la alimentacion.

Salida:
  sim/out/*.png                capturas de la pantalla virtual 480x800
  sim/out/resumen.txt          log completo con [PASS]/[FAIL]
  docs/capturas/*.png          hojas de contacto para la documentacion

Codigo de salida 0 solo si todas las verificaciones pasan.
Uso: python3 sim/run_tests.py [--lvgl-dir RUTA] [--no-build]
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")
OUT = os.path.join(HERE, "out")
DOCS = os.path.join(ROOT, "docs", "capturas")

# (escenario, archivo de estado) en orden: el estado pasa de uno a otro
SCENARIOS = [
    ("boot_fresh", "flow.bin"),
    ("flow", "flow.bin"),
    ("resume", "flow.bin"),
    ("boot_complete", "flow.bin"),
    ("powercut_lang", "cut.bin"),
    ("powercut_lang_resume", "cut.bin"),
    ("stress", "stress.bin"),
    ("matrix", "matrix.bin"),
]


def build(lvgl_dir):
    cmd = ["cmake", "-S", HERE, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Debug"]
    if lvgl_dir:
        cmd.append("-DLVGL_DIR=" + lvgl_dir)
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
    subprocess.check_call(["cmake", "--build", BUILD, "-j", str(os.cpu_count() or 2)], stdout=subprocess.DEVNULL)


def to_png():
    from PIL import Image
    for f in glob.glob(os.path.join(OUT, "*.ppm")):
        Image.open(f).save(f[:-4] + ".png")
        os.remove(f)


def contact_sheets():
    from PIL import Image, ImageDraw
    os.makedirs(DOCS, exist_ok=True)

    def sheet(files, name, cols, scale=0.5):
        files = [os.path.join(OUT, f) for f in files if os.path.exists(os.path.join(OUT, f))]
        if not files:
            return
        w, h = int(480 * scale), int(800 * scale)
        rows = (len(files) + cols - 1) // cols
        s = Image.new("RGB", (cols * (w + 6) + 6, rows * (h + 26) + 6), (40, 44, 52))
        d = ImageDraw.Draw(s)
        for i, f in enumerate(files):
            x, y = 6 + (i % cols) * (w + 6), 6 + (i // cols) * (h + 26)
            s.paste(Image.open(f).resize((w, h), Image.LANCZOS), (x, y))
            d.text((x + 4, y + h + 6), os.path.basename(f)[:-4], fill=(220, 225, 235))
        s.save(os.path.join(DOCS, name))

    langs = ["es", "en", "pt-BR", "fr", "de"]
    scr = ["welcome", "language", "privacy", "personalize"]
    for th in ("light", "dark"):
        sheet(["m_%s_%s_%s.png" % (s, l, th) for l in langs for s in scr], "matriz_idiomas_%s.png" % th, 4, 0.4)
    sheet(["boot_%04d.png" % t for t in (300, 600, 900, 1200, 1500, 1800, 2000, 2200, 2700)],
          "animacion_arranque.png", 9, 0.35)
    sheet(["01_welcome_es_light.png", "02_language_es_light.png", "03_privacy_es_light.png",
           "04_personalize_es_light.png"], "asistente_es_claro.png", 4)
    sheet(["04_personalize_es_light.png", "04_personalize_es_dark.png", "04_personalize_es_auto.png",
           "04_personalize_accent1.png", "04_personalize_accent5.png"], "temas_y_acentos.png", 5, 0.4)
    sheet(["04_personalize_wp%d_light.png" % i for i in range(1, 6)] + ["04_personalize_wp_crossfade_mid.png"],
          "fondos.png", 6, 0.35)
    sheet(["transition_mid_welcome_to_language.png", "03b_privacy_sheet_es_light.png",
           "03c_privacy_checked_es_light.png", "03_privacy_es_dark_green.png", "05_resume_personalize.png",
           "06_after_handoff_backdrop.png", "07_boot_complete_logo_hold.png", "08_resume_language_de.png"],
          "transiciones_y_estados.png", 4, 0.4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lvgl-dir")
    ap.add_argument("--no-build", action="store_true")
    a = ap.parse_args()
    if not a.no_build:
        build(a.lvgl_dir)
    os.makedirs(OUT, exist_ok=True)
    for f in glob.glob(os.path.join(OUT, "*")):
        os.remove(f)
    exe = os.path.join(BUILD, "advenya_sim")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=1", UBSAN_OPTIONS="halt_on_error=1")
    log, failed = [], False
    for name, state in SCENARIOS:
        p = subprocess.run([exe, name, os.path.join(OUT, state), OUT], cwd=OUT, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=1800)
        lines = [l for l in p.stdout.splitlines() if not l.startswith("[Warn]")]
        log += lines + ["  (codigo de salida %d)" % p.returncode, ""]
        print("\n".join(lines))
        if p.returncode != 0:
            failed = True
    total_pass = sum(l.count("[PASS]") for l in log)
    total_fail = sum(l.count("[FAIL]") for l in log)
    summary = "TOTAL: %d verificaciones correctas, %d fallidas" % (total_pass, total_fail)
    print(summary)
    with open(os.path.join(OUT, "resumen.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(log + [summary]) + "\n")
    to_png()
    contact_sheets()
    sys.exit(1 if failed or total_fail else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ADVENYA setup - generador de assets graficos
============================================

Produce, a partir de las imagenes de referencia de `assets/reference/`, todos
los recursos graficos que usa el firmware y el simulador:

  * Logotipo de arranque separado en CAPAS (A, ondas, orbita, estrella).
    No se redibuja: cada capa es un subconjunto exacto de los pixeles del
    logotipo original (`logo_boot_reference.png`). La union de las capas es el
    logotipo original y, al final de la animacion, se muestra la imagen
    completa (`img_logo_mark`) para garantizar que no hay costuras.
  * Mapas de "orden de revelado" de cada capa (A8), que el firmware usa para
    dibujar progresivamente cada elemento sin recalcular geometria.
  * Logotipo con la palabra ADVENYA (lockup) extraido de la referencia de
    bienvenida, a los tamanos que usan las pantallas.
  * Iconos de linea, banderas, los cinco fondos de pantalla de la referencia
    de personalizacion, sus miniaturas y la mascara de "velo" del fondo del
    asistente.

Todo se escribe como arrays C de LVGL 9 en
`components/advenya_assets/src/generated/` y como PNG de control en
`assets/generated/`.

Uso:   python3 tools/gen_assets.py
Requiere: Python 3.9+, numpy, Pillow.
"""
import math
import os
from collections import deque

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "assets", "reference")
OUT_C = os.path.join(ROOT, "components", "advenya_assets", "src", "generated")
OUT_PNG = os.path.join(ROOT, "assets", "generated")

SCR_W, SCR_H = 480, 800

# ---------------------------------------------------------------------------
# Escritura de arrays C (formato lv_image_dsc_t de LVGL 9.2)
# ---------------------------------------------------------------------------


class CWriter:
    def __init__(self, fname, comment):
        self.fname = fname
        self.parts = [
            "/*\n * ARCHIVO GENERADO por tools/gen_assets.py - NO EDITAR A MANO.\n"
            " * %s\n */\n#include \"advenya_assets.h\"\n\n" % comment
        ]
        self.decls = []

    @staticmethod
    def _bytes(data):
        out = []
        for i in range(0, len(data), 24):
            out.append("    " + ",".join("0x%02x" % b for b in data[i:i + 24]) + ",")
        return "\n".join(out)

    def raw(self, name, data, comment=""):
        data = bytes(data)
        self.parts.append("/* %s */\nconst uint8_t %s[%d] = {\n%s\n};\n\n" % (
            comment, name, len(data), self._bytes(data)))
        self.decls.append("extern const uint8_t %s[%d];" % (name, len(data)))

    def image(self, name, cf, w, h, data, stride, comment=""):
        data = bytes(data)
        self.parts.append(
            "/* %s (%dx%d) */\nstatic const uint8_t %s_map[%d] LV_ATTRIBUTE_LARGE_CONST = {\n%s\n};\n"
            "const lv_image_dsc_t %s = {\n"
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
            "    .header.cf = %s,\n"
            "    .header.w = %d,\n    .header.h = %d,\n    .header.stride = %d,\n"
            "    .data_size = %d,\n    .data = %s_map,\n};\n\n" % (
                comment, w, h, name, len(data), self._bytes(data), name, cf, w, h, stride,
                len(data), name))
        self.decls.append("extern const lv_image_dsc_t %s;" % name)

    def define(self, line):
        self.decls.append(line)

    def save(self):
        with open(os.path.join(OUT_C, self.fname), "w", encoding="utf-8") as f:
            f.write("".join(self.parts))
        return self.decls


def a8_bytes(alpha):
    return np.clip(np.round(alpha * 255.0), 0, 255).astype(np.uint8).tobytes()


BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], float) / 16.0 - 0.47


def rgb565_bytes(rgb, dither=True):
    """rgb: float array HxWx3 en 0..255. Devuelve bytes RGB565 little-endian."""
    h, w, _ = rgb.shape
    v = rgb.copy()
    if dither:
        th = np.tile(BAYER4, (h // 4 + 1, w // 4 + 1))[:h, :w]
        v[..., 0] += th * 8.0
        v[..., 1] += th * 4.0
        v[..., 2] += th * 8.0
    v = np.clip(np.round(v), 0, 255).astype(np.uint16)
    px = ((v[..., 0] >> 3) << 11) | ((v[..., 1] >> 2) << 5) | (v[..., 2] >> 3)
    return px.astype("<u2").tobytes()


def rgb565a8_bytes(rgb, alpha):
    return rgb565_bytes(rgb) + a8_bytes(alpha)


def save_png(arr, name, mode=None):
    os.makedirs(OUT_PNG, exist_ok=True)
    if arr.dtype != np.uint8:
        arr = np.clip(np.round(arr), 0, 255).astype(np.uint8)
    Image.fromarray(arr, mode).save(os.path.join(OUT_PNG, name))


def resize_f(arr, size, resample=Image.LANCZOS):
    """Redimensiona un array float (2D) con PIL en modo 'F'."""
    return np.array(Image.fromarray(arr.astype(np.float32), "F").resize(size, resample))


def resize_rgb(rgb, size, resample=Image.LANCZOS):
    return np.dstack([resize_f(rgb[..., c], size, resample) for c in range(3)])


def hexc(s):
    s = s.lstrip("#")
    return np.array([int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)], float)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    t = np.asarray(t)[..., None] if np.ndim(t) else t
    return a * (1 - t) + b * t


# ---------------------------------------------------------------------------
# Componentes conexos (BFS) y dilatacion, sin dependencias externas
# ---------------------------------------------------------------------------


def label_components(mask):
    h, w = mask.shape
    lab = np.zeros((h, w), np.int32)
    n = 0
    for y in range(h):
        for x in range(w):
            if mask[y, x] and not lab[y, x]:
                n += 1
                q = deque([(y, x)])
                lab[y, x] = n
                while q:
                    cy, cx = q.popleft()
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not lab[ny, nx]:
                                lab[ny, nx] = n
                                q.append((ny, nx))
    return lab, n


def grow_labels(lab, region):
    """Asigna a cada pixel de `region` sin etiqueta la etiqueta mas cercana (BFS)."""
    h, w = lab.shape
    out = lab.copy()
    q = deque(zip(*np.nonzero(out)))
    while q:
        y, x = q.popleft()
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and region[ny, nx] and not out[ny, nx]:
                out[ny, nx] = out[y, x]
                q.append((ny, nx))
    return out


def dilate(mask, r):
    out = mask.copy()
    for _ in range(r):
        m = out.copy()
        m[1:, :] |= out[:-1, :]
        m[:-1, :] |= out[1:, :]
        m[:, 1:] |= out[:, :-1]
        m[:, :-1] |= out[:, 1:]
        out = m
    return out


def fit_ellipse(xs, ys):
    """Ajuste directo de elipse (Fitzgibbon 1999). Devuelve cx, cy, a, b, theta."""
    x = xs.astype(float)
    y = ys.astype(float)
    mx, my = x.mean(), y.mean()
    sx, sy = x.std(), y.std()
    x = (x - mx) / sx
    y = (y - my) / sy
    D = np.vstack([x * x, x * y, y * y, x, y, np.ones_like(x)]).T
    S = D.T @ D
    C = np.zeros((6, 6))
    C[0, 2] = C[2, 0] = 2
    C[1, 1] = -1
    evals, evecs = np.linalg.eig(np.linalg.solve(S, C))
    idx = np.argmax(np.real(evals))
    p = np.real(evecs[:, idx])
    A, B, Cc, Dd, E, F = p
    # deshacer normalizacion
    A2 = A / (sx * sx)
    B2 = B / (sx * sy)
    C2 = Cc / (sy * sy)
    D2 = -2 * A * mx / (sx * sx) - B * my / (sx * sy) + Dd / sx
    E2 = -2 * Cc * my / (sy * sy) - B * mx / (sx * sy) + E / sy
    F2 = (A * mx * mx / (sx * sx) + B * mx * my / (sx * sy) + Cc * my * my / (sy * sy)
          - Dd * mx / sx - E * my / sy + F)
    A, B, Cc, Dd, E, F = A2, B2, C2, D2, E2, F2
    den = B * B - 4 * A * Cc
    cx = (2 * Cc * Dd - B * E) / den
    cy = (2 * A * E - B * Dd) / den
    theta = 0.5 * math.atan2(B, A - Cc)
    ct, st = math.cos(theta), math.sin(theta)
    Ap = A * ct * ct + B * ct * st + Cc * st * st
    Cp = A * st * st - B * ct * st + Cc * ct * ct
    Fp = A * cx * cx + B * cx * cy + Cc * cy * cy + Dd * cx + E * cy + F
    a = math.sqrt(-Fp / Ap)
    b = math.sqrt(-Fp / Cp)
    return cx, cy, a, b, theta


# ---------------------------------------------------------------------------
# 1) Logotipo de arranque en capas
# ---------------------------------------------------------------------------

BOOT_CROP = (152, 432, 534, 680)          # caja en logo_boot_reference.png (663x1106)
BOOT_SCALE = SCR_W / 663.0                # la referencia es una maqueta a escala de la pantalla
BOOT_SIZE = (int(round((BOOT_CROP[2] - BOOT_CROP[0]) * BOOT_SCALE)),
             int(round((BOOT_CROP[3] - BOOT_CROP[1]) * BOOT_SCALE)))


def build_boot_logo(cw, decls_extra):
    src = np.array(Image.open(os.path.join(REF, "logo_boot_reference.png")).convert("L")).astype(float)
    x0, y0, x1, y1 = BOOT_CROP
    lum = src[y0:y1, x0:x1] / 255.0
    alpha = np.clip((lum - 0.03) / 0.94, 0, 1)
    H, W = alpha.shape

    lab, n = label_components(alpha > 0.43)
    sizes = [(lab == i).sum() for i in range(1, n + 1)]
    # descartar motas
    for i, s in enumerate(sizes, 1):
        if s < 30:
            lab[lab == i] = 0
    lab = grow_labels(lab, alpha > 0.01)

    def bbox(c):
        ys, xs = np.nonzero(lab == c)
        return xs.min(), xs.max(), ys.min(), ys.max(), len(xs)

    comps = {i: bbox(i) for i in range(1, n + 1) if (lab == i).any()}
    # Identificacion por geometria (robusta frente al orden de etiquetado):
    #  orbita+estrella = componente mas ancho; ondas = componentes alargados
    #  horizontalmente que cruzan la A; el resto = A.
    orbit_c = max(comps, key=lambda c: comps[c][1] - comps[c][0])
    rest = [c for c in comps if c != orbit_c]
    apex_c = min(rest, key=lambda c: comps[c][2])                 # el mas alto: vertice de la A + 1a onda
    bands = sorted([c for c in rest if c != apex_c and
                    (comps[c][1] - comps[c][0]) > 1.6 * (comps[c][3] - comps[c][2]) and
                    (comps[c][1] - comps[c][0]) > 120],
                   key=lambda c: comps[c][2])                       # 2a y 3a onda
    assert len(bands) == 2, "no se identificaron las ondas: %r" % comps
    legs = [c for c in rest if c not in bands and c != apex_c]

    # --- separar la 1a onda del vertice de la A ---
    def top_edge(c):
        t = np.full(W, -1)
        for x in range(W):
            ys = np.nonzero(lab[:, x] == c)[0]
            if len(ys):
                t[x] = ys.min()
        return t

    t2, t3 = top_edge(bands[0]), top_edge(bands[1])
    valid = (t2 >= 0) & (t3 >= 0)
    xs_v = np.nonzero(valid)[0]
    thr = np.interp(np.arange(W), xs_v, (t2 - (t3 - t2))[valid])  # borde superior de la 1a onda
    yy, xx = np.mgrid[0:H, 0:W]
    apex = lab == apex_c
    band1 = apex & (yy >= thr[None, :] - 1)
    a_top = apex & ~band1

    # --- separar la estrella de la orbita ---
    ring = lab == orbit_c
    ox0, ox1, oy0, oy1, _ = comps[orbit_c]
    # La estrella es la zona del anillo con rayos largos horizontales/verticales:
    # su centro es la fila y la columna con mas pixeles del anillo en el cuarto
    # superior derecho.
    qy = slice(oy0, oy0 + (oy1 - oy0) // 2)
    qx = slice(ox0 + (ox1 - ox0) * 3 // 5, ox1 + 1)
    sub = ring[qy, qx]
    sy = int(np.argmax(sub.sum(1))) + qy.start
    sx = int(np.argmax(sub.sum(0))) + qx.start
    # ajuste de la elipse con los pixeles del anillo lejos de la estrella
    far = ring & ((np.abs(xx - sx) > 34) | (np.abs(yy - sy) > 34))
    fy, fx = np.nonzero(far & (alpha > 0.5))
    cx, cy, ea, eb, th = fit_ellipse(fx, fy)
    ct, st = math.cos(th), math.sin(th)
    dx, dy = xx - cx, yy - cy
    u = (dx * ct + dy * st) / ea
    v = (-dx * st + dy * ct) / eb
    rho = np.sqrt(u * u + v * v)
    dist = np.abs(rho - 1.0) * min(ea, eb)
    on_ring = dist < 4.2
    star = ring & (~on_ring | ((xx - sx) ** 2 + (yy - sy) ** 2 < 8 ** 2))
    star &= (np.abs(xx - sx) < 60) & (np.abs(yy - sy) < 60)
    orbit = ring & ~star
    star_core = ring & ((xx - sx) ** 2 + (yy - sy) ** 2 < 8 ** 2)
    orbit |= star_core            # el anillo pasa por el centro de la estrella

    a_mask = a_top.copy()
    for c in legs:
        a_mask |= lab == c
    waves_mask = band1 | (lab == bands[0]) | (lab == bands[1])
    wave_idx = np.zeros((H, W), np.uint8)
    wave_idx[lab == bands[0]] = 1
    wave_idx[lab == bands[1]] = 2

    # Solape de 2 px DENTRO del blanco original: evita costuras antialias entre
    # capas contiguas (p. ej. barra de la A / 1a onda) al componerlas.
    inside = alpha > 0.02
    layers = {
        "a": a_mask, "waves": waves_mask, "orbit": orbit, "star": star,
    }
    out_alpha = {}
    for k, m in layers.items():
        md = dilate(m, 2) & inside
        out_alpha[k] = np.clip(resize_f(alpha * md, BOOT_SIZE), 0, 1)
    full = np.clip(resize_f(alpha, BOOT_SIZE), 0, 1)

    # --- mapas de orden (0..1) calculados a resolucion de dispositivo ---
    BW, BH = BOOT_SIZE
    gy, gx = np.mgrid[0:BH, 0:BW]
    sxs = (gx + 0.5) / BOOT_SCALE - 0.5        # coordenadas en el recorte fuente
    sys_ = (gy + 0.5) / BOOT_SCALE - 0.5

    # A: desde el centro hacia fuera
    ay, ax = np.nonzero(a_mask)
    acx, acy = ax.mean(), ay.mean()
    ad = np.sqrt((sxs - acx) ** 2 + (sys_ - acy) ** 2)
    order_a = ad / ad[out_alpha["a"] > 0.02].max()

    # Ondas: izquierda -> derecha, la 2a y 3a con un ligero retraso
    wy, wx = np.nonzero(waves_mask)
    widx = np.array(Image.fromarray(wave_idx).resize(BOOT_SIZE, Image.NEAREST)).astype(float)
    order_w = (sxs - wx.min()) / float(wx.max() - wx.min()) * 0.82 + widx * 0.09

    # Orbita: por angulo sobre la elipse ajustada. Empieza justo despues de la
    # estrella y termina en ella (la estrella destella al cerrarse la orbita).
    ddx, ddy = sxs - cx, sys_ - cy
    uu = (ddx * ct + ddy * st) / ea
    vv = (-ddx * st + ddy * ct) / eb
    phi = np.arctan2(vv, uu)
    phi_star = math.atan2((-(sx - cx) * st + (sy - cy) * ct) / eb, ((sx - cx) * ct + (sy - cy) * st) / ea)
    direction = -1.0                         # sentido de recorrido
    order_o = np.mod((phi - phi_star) * direction - 0.02, 2 * math.pi) / (2 * math.pi)

    # Estrella: desde su centro
    ssx, ssy = sx, sy
    sd = np.sqrt((sxs - ssx) ** 2 + (sys_ - ssy) ** 2)
    order_s = sd / max(1.0, sd[out_alpha["star"] > 0.02].max())

    orders = {"a": order_a, "waves": order_w, "orbit": order_o, "star": order_s}
    for k in orders:
        o = np.clip(orders[k], 0, 1)
        o[out_alpha[k] <= 0.0] = 1.0
        orders[k] = o

    for k in ("a", "waves", "orbit", "star"):
        cw.image("img_boot_%s" % k, "LV_COLOR_FORMAT_A8", BW, BH, a8_bytes(out_alpha[k]), BW,
                 "capa '%s' del logotipo de arranque (alfa)" % k)
        cw.raw("boot_order_%s" % k, a8_bytes(orders[k]),
               "mapa de orden de revelado de la capa '%s' (0 = primero)" % k)
    cw.image("img_logo_mark", "LV_COLOR_FORMAT_A8", BW, BH, a8_bytes(full), BW,
             "logotipo completo (alfa), identico a la union de las capas")

    # posicion de la estrella en el lienzo de dispositivo (para el destello)
    star_dev = ((ssx + 0.5) * BOOT_SCALE, (ssy + 0.5) * BOOT_SCALE)
    decls_extra += [
        "#define ADV_BOOT_LOGO_W %d" % BW,
        "#define ADV_BOOT_LOGO_H %d" % BH,
        "#define ADV_BOOT_STAR_X %d" % int(round(star_dev[0])),
        "#define ADV_BOOT_STAR_Y %d" % int(round(star_dev[1])),
    ]

    # --- PNG de control ---
    colors = {"a": (255, 255, 255), "waves": (90, 170, 255), "orbit": (255, 200, 80), "star": (255, 90, 160)}
    prev = np.zeros((BH, BW, 3))
    for k in ("orbit", "a", "waves", "star"):
        prev = prev * (1 - out_alpha[k][..., None]) + np.array(colors[k]) * out_alpha[k][..., None]
    save_png(prev, "boot_layers_colored.png")
    save_png(full * 255, "boot_logo_full.png")
    comp = np.zeros((BH, BW))
    for k in ("orbit", "a", "waves", "star"):
        comp = comp * (1 - out_alpha[k]) + out_alpha[k]
    save_png(np.abs(comp - full) * 255 * 8, "boot_layers_union_diff_x8.png")
    print("  logo: diferencia maxima union-capas vs original = %.4f" % np.abs(comp - full).max())
    # tira de fotogramas simulando la secuencia
    frames = []
    for t in (0.3, 0.6, 0.9, 1.2, 1.5, 1.8, 2.1, 2.4, 3.0):
        f = np.zeros((BH, BW))
        for k, (s0, s1) in (("a", (0.0, 0.6)), ("waves", (0.6, 1.2)), ("orbit", (1.2, 1.8)), ("star", (1.8, 2.4))):
            p = np.clip((t - s0) / (s1 - s0), 0, 1)
            soft = 0.12
            r = np.clip((p * (1 + soft) - orders[k]) / soft, 0, 1)
            f = f * (1 - out_alpha[k] * r) + out_alpha[k] * r
        frames.append(f)
    save_png(np.hstack(frames) * 255, "boot_sequence_preview.png")


# ---------------------------------------------------------------------------
# 2) Lockup (simbolo + ADVENYA) desde la referencia de bienvenida
# ---------------------------------------------------------------------------

def build_lockup(cw, decls_extra):
    src = np.array(Image.open(os.path.join(REF, "ref_01_bienvenida.png")).convert("L")).astype(float)
    x0, y0, x1, y1 = 219, 453, 758, 825
    lum = src[y0:y1, x0:x1]
    alpha = np.clip((246.0 - lum) / (246.0 - 22.0), 0, 1)
    h, w = alpha.shape
    for name, width in (("lg", 262), ("md", 150), ("sm", 118)):
        hh = int(round(h * width / w))
        a = np.clip(resize_f(alpha, (width, hh)), 0, 1)
        cw.image("img_lockup_%s" % name, "LV_COLOR_FORMAT_A8", width, hh, a8_bytes(a), width,
                 "simbolo + palabra ADVENYA (alfa), extraido de ref_01_bienvenida.png")
        save_png(255 - a * 255, "lockup_%s.png" % name)


# ---------------------------------------------------------------------------
# 3) Iconos de linea (A8), dibujados con supermuestreo 8x
# ---------------------------------------------------------------------------

SS = 8


class Pen:
    def __init__(self, size):
        self.size = size
        self.im = Image.new("L", (size * SS, size * SS), 0)
        self.d = ImageDraw.Draw(self.im)

    def P(self, x, y):
        return (x * SS, y * SS)

    def line(self, pts, w):
        pts = [self.P(*p) for p in pts]
        self.d.line(pts, fill=255, width=int(w * SS), joint="curve")
        r = w * SS / 2
        for (x, y) in (pts[0], pts[-1]):
            self.d.ellipse((x - r, y - r, x + r, y + r), fill=255)

    def poly_outline(self, pts, w):
        self.line(list(pts) + [pts[0], pts[1]], w)

    def circle(self, cx, cy, r, w=None, fill=False):
        b = (SS * (cx - r), SS * (cy - r), SS * (cx + r), SS * (cy + r))
        if fill:
            self.d.ellipse(b, fill=255)
        else:
            self.d.ellipse(b, outline=255, width=int(w * SS))

    def rrect(self, x0, y0, x1, y1, r, w=None, fill=False):
        b = (SS * x0, SS * y0, SS * x1, SS * y1)
        if fill:
            self.d.rounded_rectangle(b, radius=r * SS, fill=255)
        else:
            self.d.rounded_rectangle(b, radius=r * SS, outline=255, width=int(w * SS))

    def erase_circle(self, cx, cy, r):
        self.d.ellipse((SS * (cx - r), SS * (cy - r), SS * (cx + r), SS * (cy + r)), fill=0)

    def alpha(self):
        return np.array(self.im.resize((self.size, self.size), Image.BOX)).astype(float) / 255.0


def bezier(p0, p1, p2, n=24):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in np.linspace(0, 1, n)]


def build_icons(cw):
    icons = {}
    # Escudo (privacidad)
    p = Pen(30)
    pts = [(15, 4)] + bezier((15, 4), (20, 7.5), (25, 7.5))[1:] + [(25, 14)] + \
        bezier((25, 14), (24.5, 23), (15, 27))[1:] + bezier((15, 27), (5.5, 23), (5, 14))[1:] + \
        [(5, 7.5)] + bezier((5, 7.5), (10, 7.5), (15, 4))[1:]
    p.line(pts, 2.4)
    icons["shield"] = p
    # Capas (permisos)
    p = Pen(30)
    p.poly_outline([(15, 4.5), (26, 10), (15, 15.5), (4, 10)], 2.2)
    p.line([(4, 15), (15, 20.5), (26, 15)], 2.2)
    p.line([(4, 20), (15, 25.5), (26, 20)], 2.2)
    icons["layers"] = p
    # Documento (condiciones)
    p = Pen(30)
    p.rrect(5.5, 4.5, 24.5, 25.5, 4, w=2.3)
    for yy in (11, 15.5):
        p.line([(10, yy), (20, yy)], 2.2)
    p.line([(10, 20), (16, 20)], 2.2)
    icons["doc"] = p
    # Apariencia (sol con mitad rellena)
    p = Pen(28)
    p.circle(14, 14, 6.2, w=2.0)
    p.d.pieslice((SS * 7.8, SS * 7.8, SS * 20.2, SS * 20.2), 90, 270, fill=255)
    for i in range(12):
        ang = i * math.pi / 6
        r0, r1 = 9.4, 11.6
        p.line([(14 + r0 * math.cos(ang), 14 + r0 * math.sin(ang)),
                (14 + r1 * math.cos(ang), 14 + r1 * math.sin(ang))], 1.7)
    icons["appearance"] = p
    # Paleta
    p = Pen(28)
    pts = []
    for i in range(0, 301, 6):
        ang = math.radians(i + 120)
        pts.append((14 + 11 * math.cos(ang), 14 + 11 * math.sin(ang)))
    pts += bezier(pts[-1], (17, 20), (19.5, 22))[1:] + bezier((19.5, 22), (19, 25.5), pts[0])[1:]
    p.line(pts, 2.1)
    for (cx, cy) in ((9, 11), (13.5, 7.8), (18.6, 9.2), (8.6, 16.4)):
        p.circle(cx, cy, 1.7, fill=True)
    icons["palette"] = p
    # Imagen (fondo de pantalla)
    p = Pen(28)
    p.rrect(4, 5, 24, 23, 3.5, w=2.1)
    p.circle(10, 10.6, 2.0, w=1.8)
    p.line([(5.5, 20.5), (11, 15), (15, 19), (18.5, 15.5), (22.5, 19.5)], 2.0)
    icons["image"] = p
    # Chevron
    p = Pen(18)
    p.line([(6.5, 3.5), (12, 9), (6.5, 14.5)], 2.0)
    icons["chevron"] = p
    # Check (para circulos de seleccion)
    p = Pen(18)
    p.line([(4, 9.5), (7.6, 13), (14, 5.5)], 2.4)
    icons["check"] = p
    # Flecha (botones)
    p = Pen(26)
    p.line([(4, 13), (22, 13)], 2.4)
    p.line([(15, 6), (22, 13), (15, 20)], 2.4)
    icons["arrow"] = p
    # Sol
    p = Pen(26)
    p.circle(13, 13, 4.6, w=1.9)
    for i in range(8):
        ang = i * math.pi / 4
        p.line([(13 + 7.4 * math.cos(ang), 13 + 7.4 * math.sin(ang)),
                (13 + 10 * math.cos(ang), 13 + 10 * math.sin(ang))], 1.9)
    icons["sun"] = p
    # Luna (creciente relleno)
    p = Pen(26)
    p.circle(12.5, 13, 8.5, fill=True)
    p.erase_circle(17.5, 9.5, 7.6)
    icons["moon"] = p

    sheet = []
    for k, pen in icons.items():
        a = pen.alpha()
        cw.image("img_ic_%s" % k, "LV_COLOR_FORMAT_A8", pen.size, pen.size, a8_bytes(a), pen.size,
                 "icono '%s'" % k)
        pad = np.zeros((32, 32))
        pad[:pen.size, :pen.size] = a
        sheet.append(pad)
    save_png(255 - np.hstack(sheet) * 255, "icons_sheet.png")


# ---------------------------------------------------------------------------
# 4) Banderas (RGB565A8 con esquinas redondeadas)
# ---------------------------------------------------------------------------

FLAG_W, FLAG_H, FLAG_R = 42, 28, 5


def flag_canvas():
    return Image.new("RGB", (FLAG_W * SS, FLAG_H * SS), (0, 0, 0))


def build_flags(cw):
    W, H = FLAG_W * SS, FLAG_H * SS
    flags = {}
    # Espana
    im = flag_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, H), fill=(198, 11, 30))
    d.rectangle((0, H // 4, W, H * 3 // 4), fill=(255, 196, 0))
    flags["es"] = im
    # Reino Unido
    im = flag_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, H), fill=(1, 33, 105))
    wd = int(H * 0.2)
    d.line((0, 0, W, H), fill=(255, 255, 255), width=wd)
    d.line((0, H, W, 0), fill=(255, 255, 255), width=wd)
    wr = int(H * 0.07)
    # diagonales rojas desplazadas (contracambiadas)
    off = wr // 2 + 2
    d.line((0, -off, W // 2, H // 2 - off), fill=(200, 16, 46), width=wr)
    d.line((W // 2, H // 2 + off, W, H + off), fill=(200, 16, 46), width=wr)
    d.line((0, H + off, W // 2, H // 2 + off), fill=(200, 16, 46), width=wr)
    d.line((W // 2, H // 2 - off, W, -off), fill=(200, 16, 46), width=wr)
    cwd = int(H * 0.33)
    d.rectangle((W // 2 - cwd // 2, 0, W // 2 + cwd // 2, H), fill=(255, 255, 255))
    d.rectangle((0, H // 2 - cwd // 2, W, H // 2 + cwd // 2), fill=(255, 255, 255))
    crd = int(H * 0.2)
    d.rectangle((W // 2 - crd // 2, 0, W // 2 + crd // 2, H), fill=(200, 16, 46))
    d.rectangle((0, H // 2 - crd // 2, W, H // 2 + crd // 2), fill=(200, 16, 46))
    flags["gb"] = im
    # Brasil
    im = flag_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, H), fill=(0, 156, 59))
    mx, my = W * 0.085, H * 0.13
    d.polygon([(W / 2, my), (W - mx, H / 2), (W / 2, H - my), (mx, H / 2)], fill=(255, 223, 0))
    r = H * 0.25
    d.ellipse((W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r), fill=(0, 39, 118))
    d.arc((W / 2 - r * 1.9, H / 2 - r * 0.55, W / 2 + r * 1.35, H / 2 + r * 2.6), 222, 302,
          fill=(255, 255, 255), width=int(H * 0.05))
    flags["br"] = im
    # Francia
    im = flag_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W // 3, H), fill=(0, 35, 149))
    d.rectangle((W // 3, 0, 2 * W // 3, H), fill=(255, 255, 255))
    d.rectangle((2 * W // 3, 0, W, H), fill=(237, 41, 57))
    flags["fr"] = im
    # Alemania
    im = flag_canvas()
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, H // 3), fill=(0, 0, 0))
    d.rectangle((0, H // 3, W, 2 * H // 3), fill=(221, 0, 0))
    d.rectangle((0, 2 * H // 3, W, H), fill=(255, 206, 0))
    flags["de"] = im

    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, W - 1, H - 1), radius=FLAG_R * SS, fill=255)
    alpha = np.array(mask.resize((FLAG_W, FLAG_H), Image.BOX)).astype(float) / 255.0
    sheet = []
    for k, im in flags.items():
        rgb = np.array(im.resize((FLAG_W, FLAG_H), Image.BOX)).astype(float)
        cw.image("img_flag_%s" % k, "LV_COLOR_FORMAT_RGB565A8", FLAG_W, FLAG_H,
                 rgb565a8_bytes(rgb, alpha), FLAG_W * 2, "bandera '%s'" % k)
        sheet.append(np.dstack([rgb * alpha[..., None] + 255 * (1 - alpha[..., None])]))
        sheet.append(np.full((FLAG_H, 6, 3), 255.0))
    save_png(np.hstack(sheet), "flags_sheet.png")


# ---------------------------------------------------------------------------
# 5) Fondos de pantalla (los cinco de la referencia de personalizacion)
# ---------------------------------------------------------------------------

def grid(w=SCR_W, h=SCR_H):
    y, x = np.mgrid[0:h, 0:w]
    return (x + 0.5) / w, (y + 0.5) / h


def wp_blue():
    u, v = grid()
    t = np.clip(0.42 * u + 0.78 * v - 0.18, 0, 1)
    c = mix(hexc("#E6F0FD"), hexc("#9CC4F8"), smoothstep(0.0, 0.55, t))
    c = mix(c, hexc("#2C7BF4"), smoothstep(0.42, 0.85, t))
    c = mix(c, hexc("#0A5BE6"), smoothstep(0.8, 1.1, t))
    # cintas fluidas de abajo-izquierda a arriba-derecha
    for k, (off, amp, hl, sh) in enumerate(((0.0, 0.10, 0.55, 0.18), (0.11, 0.08, 0.35, 0.14), (0.21, 0.07, 0.25, 0.10))):
        cu = 1.02 - 0.78 * u ** 1.15 + amp * np.sin(math.pi * u * 1.1) + off
        d = v - cu
        below = smoothstep(-0.006, 0.03, d)
        c = mix(c, c * np.array([0.82, 0.88, 1.0]), below * sh)
        c = mix(c, hexc("#FFFFFF"), np.exp(-(d / 0.010) ** 2) * hl * (0.35 + 0.65 * u))
        c = mix(c, hexc("#DDEBFF"), np.exp(-((d + 0.035) / 0.03) ** 2) * 0.10)
    return c


def wp_violet():
    u, v = grid()
    c = mix(hexc("#8D7BF0"), hexc("#5B3FD0"), smoothstep(0.1, 1.0, 0.65 * u + 0.35 * (1 - v)))
    c = mix(c, hexc("#A893F4"), smoothstep(0.55, 1.0, v) * smoothstep(0.9, 0.2, u) * 0.85)
    cu = 0.60 - 0.16 * np.sin(math.pi * (u * 1.05 - 0.08)) + 0.10 * u
    d = v - cu
    upper = smoothstep(0.02, -0.06, d)
    c = mix(c, c * 0.88 + hexc("#3A2AA8") * 0.12, upper * smoothstep(0.3, 1.0, u) * 0.6)
    c = mix(c, hexc("#C9B8FA"), np.exp(-(d / 0.035) ** 2) * 0.45)
    c = mix(c, hexc("#F1EBFF"), np.exp(-(d / 0.006) ** 2) * 0.35)
    cu2 = 0.18 + 0.30 * (1 - u) ** 2
    d2 = v - cu2
    c = mix(c, hexc("#B7A5F6"), np.exp(-(d2 / 0.05) ** 2) * 0.25 * smoothstep(0.2, 0.9, u))
    return c


def wp_peach():
    u, v = grid()
    x, y = u * SCR_W, v * SCR_H
    c = mix(hexc("#FDE8D4"), hexc("#F8CDA4"), smoothstep(0.0, 1.0, 0.5 * u + 0.6 * v))
    cx, cy, r = SCR_W * 1.30, SCR_H * 1.02, SCR_W * 1.02
    rr = np.sqrt((x - cx) ** 2 + (y - cy) ** 2)
    d = (rr - r) / SCR_W
    inside = smoothstep(0.004, -0.004, d)
    inner = mix(hexc("#F4A660"), hexc("#C9692F"), smoothstep(0.0, 1.0, 0.5 * u + 0.6 * v - 0.1))
    c = mix(c, inner, inside)
    c = mix(c, hexc("#FFE6CC"), np.exp(-((d + 0.012) / 0.008) ** 2) * 0.75)
    c = mix(c, hexc("#FFD2A8"), np.exp(-((d - 0.03) / 0.04) ** 2) * 0.35 * (1 - inside))
    rr2 = np.sqrt((x - cx * 1.02) ** 2 + (y - cy * 1.04) ** 2)
    d2 = (rr2 - r * 0.86) / SCR_W
    c = mix(c, c * 0.9, smoothstep(0.01, -0.05, d2) * inside * 0.6)
    return c


def ridge(u, seed, base, amp, freq):
    rng = np.random.RandomState(seed)
    h = np.zeros_like(u)
    for f, a in zip(freq, amp):
        ph = rng.uniform(0, 2 * math.pi)
        h += a * np.abs(np.sin(math.pi * f * u + ph)) ** 1.3
    return base - h


def wp_night():
    u, v = grid()
    c = mix(hexc("#16264D"), hexc("#3E5287"), smoothstep(0.0, 0.62, v))
    c = mix(c, hexc("#8B87B5"), np.exp(-((v - 0.60) / 0.07) ** 2) * 0.55)
    c = mix(c, hexc("#C7A9C2"), np.exp(-((v - 0.615) / 0.025) ** 2) * 0.25 * smoothstep(0.2, 0.6, u))
    layers = [
        (ridge(u, 3, 0.640, (0.035, 0.018, 0.010), (1.1, 2.7, 6.3)), "#55679A", "#3D4F80"),
        (ridge(u, 7, 0.690, (0.090, 0.025, 0.010), (0.9, 2.1, 5.7)), "#2C3C68", "#1E2C52"),
        (ridge(u, 11, 0.780, (0.060, 0.020, 0.008), (1.3, 3.1, 7.9)), "#1A2647", "#111A36"),
        (ridge(u, 17, 0.880, (0.040, 0.015, 0.006), (0.8, 2.5, 6.0)), "#0E1630", "#091024"),
    ]
    for (rh, top, bot) in layers:
        m = smoothstep(rh - 0.0015, rh + 0.0015, v)
        col = mix(hexc(top), hexc(bot), smoothstep(0, 1, (v - rh) / 0.25))
        col = mix(col, hexc("#7F8FC0"), np.exp(-((v - rh) / 0.006) ** 2) * 0.18)
        c = mix(c, col, m)
    return c


def wp_silver():
    u, v = grid()
    x, y = u * SCR_W, v * SCR_H
    c = mix(hexc("#8495B3"), hexc("#B9C4D6"), smoothstep(0.0, 1.0, 0.7 * u + 0.3 * (1 - v)))
    c = mix(c, hexc("#6E80A2"), smoothstep(0.5, 1.0, v) * smoothstep(0.6, 0.0, u) * 0.6)
    ax, ay, ar = SCR_W * 1.42, SCR_H * 0.56, SCR_W * 1.02
    bx, by, br = SCR_W * 1.58, SCR_H * 0.50, SCR_W * 0.98
    da = (np.sqrt((x - ax) ** 2 + (y - ay) ** 2) - ar) / SCR_W
    db = (np.sqrt((x - bx) ** 2 + (y - by) ** 2) - br) / SCR_W
    inA = smoothstep(0.004, -0.004, da)
    inB = smoothstep(0.004, -0.004, db)
    cres = inA * (1 - inB)
    gloss = mix(hexc("#E9EEF6"), hexc("#FFFFFF"), smoothstep(0.0, 0.12, -da))
    c = mix(c, gloss, cres * 0.92)
    c = mix(c, hexc("#FFFFFF"), np.exp(-(da / 0.006) ** 2) * 0.6)
    c = mix(c, hexc("#5E7096"), np.exp(-((db + 0.004) / 0.012) ** 2) * inA * 0.35)
    inner = inB * smoothstep(0.0, -0.25, db)
    c = mix(c, hexc("#D3DBE8"), inner * 0.35)
    return c


WALLPAPERS = [("blue", wp_blue), ("violet", wp_violet), ("peach", wp_peach), ("night", wp_night), ("silver", wp_silver)]
WP_SRC_W, WP_SRC_H = 240, 400
THUMB_W, THUMB_H, THUMB_R = 84, 104, 14


def veil_mask():
    """Mascara del velo del asistente (1 = color del tema, 0 = fondo de pantalla).
    Reproduce el fondo de las referencias: centro limpio y trazos curvos suaves
    en las esquinas superior izquierda e inferior derecha."""
    u, v = grid(WP_SRC_W, WP_SRC_H)
    x = u
    y = v * (SCR_H / SCR_W)          # coordenadas isotropicas
    m = np.full_like(u, 0.985)
    # esquina superior izquierda: arcos centrados fuera de pantalla
    for (cx, cy, r, w, k) in ((-0.62, -0.48, 1.10, 0.035, 0.30), (-0.55, -0.70, 1.28, 0.03, 0.24),
                               (-0.85, -0.30, 1.22, 0.08, 0.16)):
        d = np.sqrt((x - cx) ** 2 + (y - cy) ** 2) - r
        m -= k * np.exp(-(d / w) ** 2) * smoothstep(0.75, 0.15, x + 0.25 * y)
    corner = smoothstep(0.30, -0.05, x * 0.8 + y * 0.6 - 0.35)
    m -= corner * 0.11
    # esquina inferior derecha
    H = SCR_H / SCR_W
    for (cx, cy, r, w, k) in ((1.62, H + 0.48, 1.10, 0.035, 0.30), (1.55, H + 0.70, 1.28, 0.03, 0.24),
                               (1.85, H + 0.30, 1.22, 0.08, 0.16)):
        d = np.sqrt((x - cx) ** 2 + (y - cy) ** 2) - r
        m -= k * np.exp(-(d / w) ** 2) * smoothstep(0.75, 0.15, (1 - x) + 0.25 * (H - y))
    corner = smoothstep(0.30, -0.05, (1 - x) * 0.8 + (H - y) * 0.6 - 0.35)
    m -= corner * 0.11
    return np.clip(m, 0.30, 1.0)


def build_wallpapers(cw, decls_extra):
    mask = veil_mask()
    cw.raw("wp_veil_mask", a8_bytes(mask), "mascara del velo del asistente, %dx%d A8" % (WP_SRC_W, WP_SRC_H))
    tmask = Image.new("L", (THUMB_W * SS, THUMB_H * SS), 0)
    ImageDraw.Draw(tmask).rounded_rectangle((0, 0, THUMB_W * SS - 1, THUMB_H * SS - 1), radius=THUMB_R * SS, fill=255)
    talpha = np.array(tmask.resize((THUMB_W, THUMB_H), Image.BOX)).astype(float) / 255.0
    thumbs_sheet = []
    previews = []
    mask_big = resize_f(mask, (SCR_W, SCR_H), Image.BILINEAR)
    for i, (name, fn) in enumerate(WALLPAPERS):
        full = np.clip(fn(), 0, 255)
        save_png(full, "wallpaper_%d_%s.png" % (i + 1, name))
        src = np.clip(resize_rgb(full, (WP_SRC_W, WP_SRC_H), Image.BOX), 0, 255)
        cw.raw("wp_src_%s" % name, np.round(src).astype(np.uint8).tobytes(),
               "fondo '%s' %dx%d RGB888 (se escala x2 en el dispositivo)" % (name, WP_SRC_W, WP_SRC_H))
        # miniatura: recorte central con la proporcion de la tarjeta
        ch = int(round(SCR_W * THUMB_H / THUMB_W))
        y0 = (SCR_H - ch) // 2
        th = np.clip(resize_rgb(full[y0:y0 + ch], (THUMB_W, THUMB_H), Image.LANCZOS), 0, 255)
        cw.image("img_thumb_%s" % name, "LV_COLOR_FORMAT_RGB565A8", THUMB_W, THUMB_H,
                 rgb565a8_bytes(th, talpha), THUMB_W * 2, "miniatura del fondo '%s'" % name)
        thumbs_sheet.append(th * talpha[..., None] + 255 * (1 - talpha[..., None]))
        thumbs_sheet.append(np.full((THUMB_H, 8, 3), 255.0))
        # mismo muestreo que el horneado del dispositivo (ui_backdrop.c): la mitad
        # superior izquierda toma el color del fondo girado 180 grados, para que
        # las dos esquinas muestren el tono caracteristico del fondo.
        uu, vv = grid()
        rot = (uu + vv) < 1.0
        srcp = np.where(rot[..., None], full[::-1, ::-1], full)
        lt = srcp * (1 - mask_big[..., None]) + np.array([255, 255, 255]) * mask_big[..., None]
        dk = srcp * (1 - mask_big[..., None]) + hexc("#0A0F1C") * mask_big[..., None]
        previews.append(np.hstack([lt, np.full((SCR_H, 10, 3), 128.0), dk]))
    save_png(np.hstack(thumbs_sheet), "wallpaper_thumbs.png")
    save_png(np.hstack([resize_rgb(p, (p.shape[1] // 2, p.shape[0] // 2)) for p in previews]),
             "backdrop_previews_light_dark.png")
    decls_extra += [
        "#define ADV_WP_SRC_W %d" % WP_SRC_W, "#define ADV_WP_SRC_H %d" % WP_SRC_H,
        "#define ADV_WP_COUNT %d" % len(WALLPAPERS),
        "#define ADV_THUMB_W %d" % THUMB_W, "#define ADV_THUMB_H %d" % THUMB_H,
    ]


def build_glow(cw):
    s = 96
    y, x = np.mgrid[0:s, 0:s]
    r = np.sqrt((x + 0.5 - s / 2) ** 2 + (y + 0.5 - s / 2) ** 2) / (s / 2)
    a = np.exp(-(r / 0.38) ** 2) * 0.85 + np.exp(-(r / 0.12) ** 2) * 0.15
    a *= smoothstep(1.0, 0.85, r)
    cw.image("img_glow", "LV_COLOR_FORMAT_A8", s, s, a8_bytes(np.clip(a, 0, 1)), s, "halo radial (destello)")


def build_theme_previews(cw):
    """Miniaturas de 'Claro', 'Oscuro' y 'Automatico' (referencia de personalizacion)."""
    W, H, R = 100, 52, 10
    w, h = W * SS, H * SS

    def rr(d, box, r, fill):
        d.rounded_rectangle([v * SS for v in box], radius=r * SS, fill=fill)

    ims = {}
    im = Image.new("RGB", (w, h), (243, 247, 253))
    d = ImageDraw.Draw(im)
    rr(d, (9, 9, 91, 21), 5, (200, 220, 248))
    rr(d, (9, 29, 44, 42), 5, (200, 220, 248))
    rr(d, (50, 29, 91, 42), 5, (222, 233, 248))
    ims["light"] = im
    im = Image.new("RGB", (w, h), (31, 39, 56))
    d = ImageDraw.Draw(im)
    rr(d, (9, 9, 91, 21), 5, (58, 68, 88))
    rr(d, (9, 29, 44, 42), 5, (58, 68, 88))
    ims["dark"] = im
    im = Image.new("RGB", (w, h), (238, 243, 250))
    d = ImageDraw.Draw(im)
    d.polygon([(60 * SS, 0), (w, 0), (w, h), (42 * SS, h)], fill=(20, 28, 46))
    d.line([(60 * SS, 0), (42 * SS, h)], fill=(30, 107, 240), width=int(3.2 * SS))
    cx, cy = 24 * SS, 26 * SS
    r = 5.2 * SS
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=(40, 44, 52), width=int(1.6 * SS))
    for i in range(8):
        a = i * math.pi / 4
        d.line([(cx + 8 * SS * math.cos(a), cy + 8 * SS * math.sin(a)),
                (cx + 11 * SS * math.cos(a), cy + 11 * SS * math.sin(a))], fill=(40, 44, 52), width=int(1.6 * SS))
    mx, my = 78 * SS, 26 * SS
    d.ellipse((mx - 9 * SS, my - 9 * SS, mx + 9 * SS, my + 9 * SS), fill=(61, 123, 245))
    d.ellipse((mx - 3 * SS, my - 13 * SS, mx + 13 * SS, my + 3 * SS), fill=(20, 28, 46))
    ims["auto"] = im
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius=R * SS, fill=255)
    alpha = np.array(mask.resize((W, H), Image.BOX)).astype(float) / 255.0
    sheet = []
    for k, im in ims.items():
        rgb = np.array(im.resize((W, H), Image.BOX)).astype(float)
        cw.image("img_theme_%s" % k, "LV_COLOR_FORMAT_RGB565A8", W, H, rgb565a8_bytes(rgb, alpha), W * 2,
                 "vista previa del tema '%s'" % k)
        sheet.append(rgb * alpha[..., None] + 255 * (1 - alpha[..., None]))
        sheet.append(np.full((H, 8, 3), 255.0))
    save_png(np.hstack(sheet), "theme_previews.png")


def main():
    os.makedirs(OUT_C, exist_ok=True)
    os.makedirs(OUT_PNG, exist_ok=True)
    decls_extra = []
    all_decls = []
    steps = [
        ("assets_logo.c", "Logotipo en capas, mapas de revelado y lockups", lambda cw: (build_boot_logo(cw, decls_extra), build_lockup(cw, decls_extra), build_glow(cw))),
        ("assets_icons.c", "Iconos de linea y banderas", lambda cw: (build_icons(cw), build_flags(cw), build_theme_previews(cw))),
        ("assets_wallpapers.c", "Fondos de pantalla, miniaturas y velo", lambda cw: build_wallpapers(cw, decls_extra)),
    ]
    for fname, comment, fn in steps:
        print("generando", fname)
        cw = CWriter(fname, comment)
        fn(cw)
        all_decls += cw.save()
    with open(os.path.join(OUT_C, "assets_gen.h"), "w", encoding="utf-8") as f:
        f.write("/* ARCHIVO GENERADO por tools/gen_assets.py - NO EDITAR A MANO. */\n#pragma once\n\n")
        f.write("\n".join(decls_extra) + "\n\n")
        f.write("\n".join(all_decls) + "\n")
    print("listo")


if __name__ == "__main__":
    main()

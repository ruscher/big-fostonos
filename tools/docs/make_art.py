#!/usr/bin/env python3
"""Artes da documentação: banner do README e o resumo da instalação em 3 etapas.

  python3 tools/docs/make_art.py          -> docs/art/banner.png, docs/art/instalacao-3-etapas.png

Usa só imagens reais do projeto (telas capturadas do agente e do BigFoston OS, capturas do
instalador) e a fonte Montserrat do próprio repositório. SPDX-License-Identifier: GPL-3.0-or-later
"""
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

TOP = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ART = os.path.join(TOP, "docs", "art")
IMG = os.path.join(TOP, "docs", "images")
MONT = os.path.join(TOP, "src", "ui", "tools", "fonts", "Montserrat-Medium.ttf")
BOLD = next((p for p in ("/usr/share/fonts/noto/NotoSans-Black.ttf", "/usr/share/fonts/noto/NotoSans-Bold.ttf",
                         "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf") if os.path.isfile(p)), MONT)
S = 2                                   # desenha em 2x e reduz: bordas suaves


def font(path, size):
    return ImageFont.truetype(path, size * S)


def gradient(w, h, c0, c1, c2):
    """Degradê diagonal em três cores (as do Dual Boot: anil, roxo e verde-água)."""
    g = Image.new("RGB", (w, h))
    px = g.load()
    for y in range(h):
        for x in range(w):
            t = (x / w * 0.75 + y / h * 0.25)
            a, b, u = (c0, c1, t * 2) if t < 0.5 else (c1, c2, (t - 0.5) * 2)
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * u) for i in range(3))
    return g


def glow(base, cx, cy, r, color, alpha):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).ellipse((cx - r, cy - r, cx + r, cy + r), fill=color + (alpha,))
    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(r // 2)))


def device(screen_path, width):
    """A tela dentro de uma moldura preta com o nome FOSTON embaixo (como o aparelho)."""
    scr = Image.open(screen_path).convert("RGBA")
    sw = width
    sh = sw * scr.height // scr.width
    scr = scr.resize((sw, sh), Image.LANCZOS)
    bez, chin = sw // 22, sw // 9
    w, h = sw + 2 * bez, sh + bez + chin
    dev = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(dev)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=bez * 2, fill=(18, 18, 22, 255), outline=(90, 92, 100, 255),
                        width=max(2, S * 2))
    dev.paste(scr, (bez, bez))
    f = font(MONT, max(8, sw // (36 * S)))
    tw = d.textlength("FOSTON", font=f)
    d.text(((w - tw) / 2, bez + sh + (chin - f.size) / 2 - S), "FOSTON", font=f, fill=(200, 202, 210, 255))
    return dev


def shadowed(canvas, img, x, y, blur=18):
    sh = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    mask = img.split()[3].point(lambda a: 150 if a else 0)
    sh.paste((0, 0, 0, 255), (x + 10 * S, y + 16 * S), mask)
    canvas.alpha_composite(sh.filter(ImageFilter.GaussianBlur(blur * S)))
    canvas.alpha_composite(img, (x, y))


def banner():
    W, H = 1280 * S, 520 * S
    c = gradient(W // 8, H // 8, (19, 27, 74), (64, 38, 128), (20, 120, 128)).resize((W, H), Image.BICUBIC)
    c = c.convert("RGBA")
    glow(c, int(W * 0.18), int(H * 0.2), 260 * S, (120, 90, 255), 90)
    glow(c, int(W * 0.85), int(H * 0.9), 300 * S, (40, 220, 190), 70)
    d = ImageDraw.Draw(c)
    mark = Image.open(os.path.join(TOP, "installer", "bigfoston_installer", "data", "images", "fostonos-mark.png"))
    mark = mark.convert("RGBA").resize((150 * S, 150 * S * mark.height // mark.width), Image.LANCZOS)
    c.alpha_composite(mark, (64 * S, 70 * S))
    d.text((64 * S, 200 * S), "BigFoston OS", font=font(BOLD, 64), fill="white")
    d.text((66 * S, 285 * S), "Linux no navegador GPS FOSTON FS-707DC", font=font(MONT, 25), fill=(225, 228, 255))
    d.text((66 * S, 322 * S), "Dual Boot com o Windows CE · sem gravar firmware · reversível",
           font=font(MONT, 18), fill=(190, 200, 235))
    x = 66 * S
    pills = Image.new("RGBA", c.size, (0, 0, 0, 0))
    pd = ImageDraw.Draw(pills)
    f = font(MONT, 16)
    spots = []
    for txt in ("Instalação pelo cartão SD", "Instalador gráfico", "Volta ao original"):
        tw = d.textlength(txt, font=f)
        pd.rounded_rectangle((x, 380 * S, x + tw + 28 * S, 416 * S), radius=18 * S, fill=(255, 255, 255, 40),
                             outline=(255, 255, 255, 110), width=S)
        spots.append((x + 14 * S, txt))
        x += tw + 40 * S
    c.alpha_composite(pills)
    d = ImageDraw.Draw(c)
    for sx, txt in spots:
        d.text((sx, 386 * S), txt, font=f, fill="white")
    back = device(os.path.join(IMG, "dual-boot", "01-contagem.png"), 430 * S)
    front = device(os.path.join(IMG, "ui", "01-inicio.png"), 470 * S)
    shadowed(c, back, 690 * S, 46 * S)
    shadowed(c, front, 760 * S, 214 * S)
    c.convert("RGB").resize((W // S, H // S), Image.LANCZOS).save(os.path.join(ART, "banner.png"), optimize=True)


def steps():
    W, H = 1280 * S, 470 * S
    c = Image.new("RGBA", (W, H), (246, 247, 251, 255))
    d = ImageDraw.Draw(c)
    cols = (
        ("1", "No computador", "O BigFoston OS Installer prepara\no cartão SD e confere tudo",
         os.path.join(IMG, "instalador", "08-resumo.png"), (53, 132, 228)),
        ("2", "No FOSTON", "“Instalar o Dual Boot”: o próprio\naparelho grava a memória interna",
         os.path.join(IMG, "dual-boot", "08-confirmar-instalar.png"), (124, 77, 255)),
        ("3", "Pronto", "Ao ligar, escolha BigFoston OS\nou o Windows CE original",
         os.path.join(IMG, "ui", "01-inicio.png"), (38, 162, 105)),
    )
    cw = (W - 4 * 36 * S) // 3
    for i, (n, title, sub, shot, color) in enumerate(cols):
        x = 36 * S + i * (cw + 36 * S)
        d.rounded_rectangle((x, 24 * S, x + cw, H - 24 * S), radius=22 * S, fill="white",
                            outline=(220, 223, 232), width=S)
        d.ellipse((x + 24 * S, 46 * S, x + 70 * S, 92 * S), fill=color)
        f = font(BOLD, 24)
        tw = d.textlength(n, font=f)
        d.text((x + 47 * S - tw / 2, 52 * S), n, font=f, fill="white")
        d.text((x + 86 * S, 52 * S), title, font=font(BOLD, 24), fill=(30, 33, 45))
        d.multiline_text((x + 24 * S, 108 * S), sub, font=font(MONT, 16), fill=(85, 90, 105), spacing=6 * S)
        img = Image.open(shot).convert("RGBA")
        iw = cw - 48 * S
        ih = iw * img.height // img.width
        maxh = H - 24 * S - 178 * S - 24 * S
        if ih > maxh:
            ih = maxh
            img = img.crop((0, 0, img.width, img.width * ih // iw))
        img = img.resize((iw, ih), Image.LANCZOS)
        m = Image.new("L", img.size, 0)
        ImageDraw.Draw(m).rounded_rectangle((0, 0, iw - 1, ih - 1), radius=12 * S, fill=255)
        img.putalpha(m)
        shadowed(c, img, x + 24 * S, 178 * S, blur=8)
        if i < 2:
            ax = x + cw + 6 * S
            d.polygon([(ax, H // 2 - 14 * S), (ax + 24 * S, H // 2), (ax, H // 2 + 14 * S)], fill=(170, 175, 190))
    c.convert("RGB").resize((W // S, H // S), Image.LANCZOS).save(os.path.join(ART, "instalacao-3-etapas.png"),
                                                                    optimize=True)


if __name__ == "__main__":
    os.makedirs(ART, exist_ok=True)
    banner()
    steps()
    print("docs/art/banner.png, docs/art/instalacao-3-etapas.png")

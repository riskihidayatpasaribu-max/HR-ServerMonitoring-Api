#!/usr/bin/env python3
"""
RH_png2rgb565.py
Ubah logo PNG (boleh transparan) menjadi array RGB565 untuk RH_Logos.h.

Transparansi digabung dulu ke warna latar tempat logo akan dipasang,
jadi tepi logo halus (anti-alias) dan tidak butuh alpha di ESP32.

Jalankan dari folder tools/ :
    python3 RH_png2rgb565.py
Butuh: pip install pillow
Ganti logo -> timpa file PNG, ubah ukuran/warna latar di bawah, jalankan ulang,
lalu salin RH_Logos.h hasilnya ke folder sketch (satu tingkat di atas).
"""
from PIL import Image

#            nama          file PNG          tinggi/ lebar tetap   warna latar (R,G,B)
LOGOS = [
    ("RSUP", "RH_logo_rsup.png", dict(h=42),  (255, 255, 255)),   # di header putih
    ("AE",   "RH_logo_ae.png",   dict(h=34),  (12, 14, 18)),      # di footer gelap (= C_BG)
]

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert(name, path, size, bg):
    im = Image.open(path).convert("RGBA")
    if "h" in size:
        h = size["h"]; w = round(im.width * h / im.height)
    else:
        w = size["w"]; h = round(im.height * w / im.width)
    im = im.resize((w, h), Image.LANCZOS)
    base = Image.new("RGBA", (w, h), bg + (255,))
    base.alpha_composite(im)
    px = [rgb565(*p[:3]) for p in base.getdata()]
    return w, h, px

out = ["// RH_Logos.h - DIHASILKAN OTOMATIS oleh tools/RH_png2rgb565.py, jangan diedit tangan.",
       "// Format: RGB565, urutan baris kiri->kanan atas->bawah. Disimpan di flash.",
       "#ifndef RH_LOGOS_H", "#define RH_LOGOS_H", "", "#include <Arduino.h>", ""]
for name, path, size, bg in LOGOS:
    w, h, px = convert(name, path, size, bg)
    out.append(f"#define RH_LOGO_{name}_W {w}")
    out.append(f"#define RH_LOGO_{name}_H {h}")
    out.append(f"static const uint16_t RH_LOGO_{name}[{w*h}] PROGMEM = {{")
    for i in range(0, len(px), 12):
        out.append("  " + ",".join(f"0x{v:04X}" for v in px[i:i+12]) + ",")
    out.append("};\n")
out.append("#endif // RH_LOGOS_H\n")
open("RH_Logos.h", "w").write("\n".join(out))
print("RH_Logos.h dibuat")

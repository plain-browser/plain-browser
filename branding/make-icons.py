"""Draws the Plain Browser logo and writes every icon the Windows build needs into branding/icons/.
Needs Pillow. The mark is a white geometric "P" on a blue disc: a stem and a bowl made of one disc.
Usage: python branding/make-icons.py"""
import os
from PIL import Image, ImageDraw, ImageFont

BLUE = (45, 108, 223, 255)
WHITE = (255, 255, 255, 255)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icons")
SS = 4  # supersampling for clean edges

# All geometry on a 256 grid.
STEM = (84, 64, 112, 192)
BOWL_BOX = (84, 64, 126, 156)       # square left half; the right half is the disc
BOWL_C, BOWL_R = (126, 110), 46
COUNTER_C, COUNTER_R = (128, 110), 16  # its left edge (112) meets the stem's right edge


def draw_mark(size, disc=True, mono=False):
    """disc: blue circle behind the P. mono: white P only, on transparent (22 px tray/mono icon)."""
    n = size * SS
    k = n / 256.0
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def box(b):
        return [b[0] * k, b[1] * k, b[2] * k, b[3] * k]

    def circle(c, r):
        return [(c[0] - r) * k, (c[1] - r) * k, (c[0] + r) * k, (c[1] + r) * k]

    fg = WHITE
    hole = BLUE if disc else (0, 0, 0, 0)
    if disc:
        d.ellipse([8 * k, 8 * k, 248 * k, 248 * k], fill=BLUE)
    d.rectangle(box(STEM), fill=fg)
    d.rectangle(box(BOWL_BOX), fill=fg)
    d.pieslice(circle(BOWL_C, BOWL_R), -90, 90, fill=fg)  # right half only: the left half would overhang the stem
    if disc:
        d.ellipse(circle(COUNTER_C, COUNTER_R), fill=hole)
    else:
        # Punch a real hole: draw the counter into a mask and clear those pixels.
        mask = Image.new("L", (n, n), 0)
        ImageDraw.Draw(mask).ellipse(circle(COUNTER_C, COUNTER_R), fill=255)
        img.paste((0, 0, 0, 0), (0, 0), mask)
    return img.resize((size, size), Image.LANCZOS)


def save_png(path, size, **kw):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    draw_mark(size, **kw).save(path)


def save_canvas(path, w, h, mark_size):
    """The mark centred on a transparent w x h canvas (wide/large tiles and the splash screen)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    m = draw_mark(mark_size)
    img.paste(m, ((w - mark_size) // 2, (h - mark_size) // 2), m)
    img.save(path)


def save_on_background(path, size, mark_size, bg=(243, 246, 251, 255)):
    """Logo centred on a solid square (Store box art: transparent logos look wrong on some Store layouts)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img = Image.new("RGBA", (size, size), bg)
    m = draw_mark(mark_size)
    img.paste(m, ((size - mark_size) // 2, (size - mark_size) // 2), m)
    img.save(path)


def wordmark(height, white):
    """Mark plus "Plain Browser" in Segoe UI (or any available font), `height` px tall, on transparent."""
    mark = draw_mark(height)
    font = None
    for name in (r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\segoeui.ttf", "DejaVuSans-Bold.ttf", "DejaVuSans.ttf"):
        try:
            font = ImageFont.truetype(name, int(height * 0.62))
            break
        except OSError:
            continue
    if font is None:
        font = ImageFont.load_default()
    text = "Plain Browser"
    box = font.getbbox(text)
    tw, th = box[2] - box[0], box[3] - box[1]
    gap = max(4, height // 4)
    img = Image.new("RGBA", (height + gap + tw + 2, height), (0, 0, 0, 0))
    img.paste(mark, (0, 0), mark)
    color = (255, 255, 255, 255) if white else (60, 64, 67, 255)
    ImageDraw.Draw(img).text((height + gap - box[0], (height - th) // 2 - box[1]), text, font=font, fill=color)
    return img


def write_theme_images():
    """Replacements for chrome/app/theme/default_{100,200}_percent/chromium/ (infobars, About page, New Tab)."""
    for pct, scale in (("100", 1), ("200", 2)):
        d = os.path.join(OUT, "theme", "default_%s_percent" % pct)
        save_png(os.path.join(d, "product_logo_16.png"), 16 * scale)
        save_png(os.path.join(d, "product_logo_32.png"), 32 * scale)
        wordmark(22 * scale, False).save(os.path.join(d, "product_logo_name_22.png"))
        wordmark(22 * scale, True).save(os.path.join(d, "product_logo_name_22_white.png"))


def write_store_images():
    """Optional Partner Center images: app tile icons and 1:1 box art."""
    d = os.path.join(OUT, "store")
    for n in (300, 150, 71):
        save_png(os.path.join(d, "store_icon_%d.png" % n), n)
    save_on_background(os.path.join(d, "box_art_1080.png"), 1080, 760)


def write_msix_assets():
    """Logos named as AppxManifest.xml references them (scripts/package-msix.py copies them into Assets/)."""
    d = os.path.join(OUT, "msix")
    save_png(os.path.join(d, "StoreLogo.png"), 50)
    save_png(os.path.join(d, "Square44x44Logo.png"), 44)
    for t in (16, 24, 32, 48, 256):
        save_png(os.path.join(d, "Square44x44Logo.targetsize-%d.png" % t), t)
        save_png(os.path.join(d, "Square44x44Logo.targetsize-%d_altform-unplated.png" % t), t)
    save_png(os.path.join(d, "Square71x71Logo.png"), 71)
    save_png(os.path.join(d, "Square150x150Logo.png"), 150)
    save_canvas(os.path.join(d, "Wide310x150Logo.png"), 310, 150, 110)
    save_png(os.path.join(d, "Square310x310Logo.png"), 310)
    save_canvas(os.path.join(d, "SplashScreen.png"), 620, 300, 200)


def main():
    write_msix_assets()
    write_store_images()
    write_theme_images()
    for s in (16, 24, 32, 48, 64, 128, 192, 256):
        save_png(os.path.join(OUT, "product_logo_%d.png" % s), s)
    save_png(os.path.join(OUT, "product_logo_22_mono.png"), 22, disc=False)
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    big = draw_mark(256)
    for name in ("chromium.ico", "chromium_doc.ico", "chromium_pdf.ico", "app_list.ico"):
        big.save(os.path.join(OUT, name), sizes=sizes)
    save_png(os.path.join(OUT, "tiles", "Logo.png"), 150)
    save_png(os.path.join(OUT, "tiles", "SmallLogo.png"), 70)
    with open(os.path.join(OUT, "product_logo.svg"), "w") as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><circle cx="128" cy="128" r="120" fill="#2D6CDF"/>'
                '<path fill="#fff" fill-rule="evenodd" d="M84 64h42a46 46 0 0 1 0 92h-14v36H84z M128 94a16 16 0 0 0 0 32a16 16 0 0 0 0-32z"/></svg>\n')
    print("wrote", OUT)


if __name__ == "__main__":
    main()

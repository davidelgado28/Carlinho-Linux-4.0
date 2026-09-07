import math
from PIL import Image, ImageDraw, ImageFilter

def create_wallpaper(output_path="/tmp/carlinho_bg.png"):
    width, height = 1920, 1080
    cx, cy = width // 2, height // 2
    img = Image.new('RGB', (width, height), color=(0, 0, 0))
    glow = Image.new('RGB', (width, height), color=(0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_radius = 220
    glow_draw.ellipse(
        (cx - glow_radius, cy - glow_radius, cx + glow_radius, cy + glow_radius),
        fill=(255, 20, 30)
    )
    glow = glow.filter(ImageFilter.GaussianBlur(60))
    img = Image.blend(img, glow, alpha=0.7)
    draw = ImageDraw.Draw(img)
    r = 110
    draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(230, 0, 35))
    c_outer_r = 55
    c_inner_r = 32
    draw.ellipse((cx - c_outer_r, cy - c_outer_r, cx + c_outer_r, cy + c_outer_r), fill=(255, 255, 255))
    draw.ellipse((cx - c_inner_r, cy - c_inner_r, cx + c_inner_r, cy + c_inner_r), fill=(230, 0, 35))
    cutout_box = (cx + 5, cy - 35, cx + c_outer_r + 15, cy + 35)
    draw.rectangle(cutout_box, fill=(230, 0, 35))

    img.save(output_path, "PNG")
    print(f"[+] Papel de parede gerado com sucesso em: {output_path}")

if __name__ == "__main__":
    create_wallpaper()

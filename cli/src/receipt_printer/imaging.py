import base64

from PIL import Image

PRINTER_WIDTH = 384  # dots


def image_to_raster(path: str) -> tuple[str, int, int]:
    """Convert an image file to 1-bit raster bitmap for ESC/POS printing.

    Returns (base64_data, width, height).
    """
    img = Image.open(path)

    # Composite alpha onto white background
    if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
        bg = Image.new("RGB", img.size, (255, 255, 255))
        bg.paste(img, mask=img.split()[-1] if img.mode in ("RGBA", "LA") else None)
        img = bg

    # Resize to printer width, maintaining aspect ratio
    ratio = PRINTER_WIDTH / img.width
    new_height = int(img.height * ratio)
    img = img.resize((PRINTER_WIDTH, new_height), Image.LANCZOS)

    # Convert to 1-bit with dithering
    img = img.convert("1")

    width = img.width
    height = img.height
    pixels = img.load()

    # Pack into bytes (MSB first, 1=black, 0=white)
    bytes_per_row = (width + 7) // 8
    bitmap = bytearray(bytes_per_row * height)

    for y in range(height):
        for x in range(width):
            # PIL "1" mode: 0=black, 255=white
            # ESC/POS: 1=black dot, 0=white
            if pixels[x, y] == 0:
                byte_idx = y * bytes_per_row + x // 8
                bit_idx = 7 - (x % 8)
                bitmap[byte_idx] |= 1 << bit_idx

    return base64.b64encode(bytes(bitmap)).decode("ascii"), width, height

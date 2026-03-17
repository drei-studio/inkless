import base64

from PIL import Image

PRINTER_WIDTH = 384  # dots


def composite_alpha_onto_white(img: Image.Image) -> Image.Image:
    """Composite transparent images onto a white background."""
    if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
        if img.mode == "P":
            img = img.convert("RGBA")
        bg = Image.new("RGB", img.size, (255, 255, 255))
        bg.paste(img, mask=img.split()[-1])
        return bg
    return img


def image_to_raster(path: str) -> tuple[str, int, int]:
    """Convert an image file to 1-bit raster bitmap for ESC/POS printing.

    Returns (base64_data, width, height).
    """
    img = Image.open(path)
    img = composite_alpha_onto_white(img)

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


def image_to_header(path: str, width: int = PRINTER_WIDTH) -> str:
    """Convert an image file to a C header with a PROGMEM bitmap array.

    Uses threshold (no dithering) for clean logo output.
    Width defaults to full printer width (384). Image is centered on the
    full printer width when smaller.
    Returns the complete header file content as a string.
    """
    img = Image.open(path)
    if img.width == 0 or img.height == 0:
        raise ValueError(f"Image has invalid dimensions: {img.width}x{img.height}")
    img = composite_alpha_onto_white(img)

    # Resize to target width, maintaining aspect ratio
    ratio = width / img.width
    new_height = int(img.height * ratio)
    img = img.resize((width, new_height), Image.LANCZOS)

    # Center on full printer width if smaller
    if width < PRINTER_WIDTH:
        centered = Image.new("RGB", (PRINTER_WIDTH, new_height), (255, 255, 255))
        offset = (PRINTER_WIDTH - width) // 2
        centered.paste(img, (offset, 0))
        img = centered

    # Convert to 1-bit with threshold (no dithering — better for logos)
    img = img.convert("L")
    img = img.point(lambda x: 0 if x < 128 else 255, mode="1")

    width = img.width
    height = img.height
    pixels = img.load()

    # Pack into bytes (MSB first, PIL 0=black → ESC/POS 1=black)
    bytes_per_row = (width + 7) // 8
    bitmap = bytearray(bytes_per_row * height)

    for y in range(height):
        for x in range(width):
            if pixels[x, y] == 0:
                byte_idx = y * bytes_per_row + x // 8
                bit_idx = 7 - (x % 8)
                bitmap[byte_idx] |= 1 << bit_idx

    # Format as C header
    lines = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        f"#define LOGO_W {width}",
        f"#define LOGO_H {height}",
        "",
        f"const uint8_t LOGO_BITMAP[{len(bitmap)}] PROGMEM = {{",
    ]

    # Format bytes in rows of 16
    for i in range(0, len(bitmap), 16):
        chunk = bitmap[i : i + 16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        comma = "," if i + 16 < len(bitmap) else ""
        lines.append(f"    {hex_vals}{comma}")

    lines.append("};")
    lines.append("")

    return "\n".join(lines)

from typing import Annotated, Optional

import typer
from rich import print as rprint

from .client import PrinterClient, DEFAULT_BASE_URL
from .formatting import print_note, print_todo
from .imaging import image_to_header, image_to_raster

app = typer.Typer(help="Receipt Printer CLI")

PrinterURL = Annotated[
    str,
    typer.Option("--printer", "-p", envvar="PRINTER_URL", help="Printer base URL"),
]


def get_client(printer: str = DEFAULT_BASE_URL) -> PrinterClient:
    return PrinterClient(printer)


@app.command()
def status(printer: PrinterURL = DEFAULT_BASE_URL):
    """Check printer status."""
    try:
        info = get_client(printer).status()
        rprint(f"[green]Online[/green] | FW {info['firmware']} | Uptime {info['uptime_s']}s")
    except Exception as e:
        rprint(f"[red]Offline[/red]: {e}")
        raise typer.Exit(1)


@app.command()
def text(
    message: str,
    bold: bool = typer.Option(False, "--bold", "-b"),
    underline: bool = typer.Option(False, "--underline", "-u"),
    align: str = typer.Option("left", "--align", "-a", help="left/center/right"),
    size: int = typer.Option(1, "--size", "-s", help="Font size multiplier (1-4)"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Print text with formatting options."""
    align_map = {"left": 0, "center": 1, "right": 2}
    client = get_client(printer)
    client.print_text(
        message,
        bold=bold,
        underline=underline,
        align=align_map.get(align, 0),
        font_width=size,
        font_height=size,
    )
    rprint("[green]Printed.[/green]")


@app.command()
def qr(
    data: str,
    label: str = typer.Option("", "--label", "-l"),
    size: int = typer.Option(4, "--size", "-s", help="Module size (1-8)"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Print a QR code."""
    get_client(printer).print_qrcode(data, label=label, size=size)
    rprint("[green]QR printed.[/green]")


@app.command()
def image(
    path: str,
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Print an image (resized, dithered to 1-bit)."""
    rprint(f"Processing {path}...")
    data_b64, width, height = image_to_raster(path)
    get_client(printer).print_image(data_b64, width, height)
    rprint(f"[green]Printed {width}x{height} image.[/green]")


@app.command()
def note(
    title: str,
    body: str,
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Print a formatted note."""
    print_note(get_client(printer), title, body)
    rprint("[green]Note printed.[/green]")


@app.command()
def todo(
    title: str,
    items: list[str] = typer.Argument(help="Checklist items"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Print a todo checklist with empty checkboxes."""
    print_todo(get_client(printer), title, items)
    rprint("[green]Todo printed.[/green]")


@app.command(name="logo-header")
def logo_header(
    image_path: str = typer.Argument(help="Path to source PNG image"),
    output: str = typer.Option("logo_data.h", "--output", "-o", help="Output header file path"),
    width: int = typer.Option(384, "--width", "-w", help="Logo width in dots (384 = full width, 128 = ~1/3)"),
):
    """Generate a logo_data.h C header from a PNG image."""
    from pathlib import Path

    if not Path(image_path).exists():
        rprint(f"[red]File not found: {image_path}[/red]")
        raise typer.Exit(1)

    try:
        header = image_to_header(image_path, width=width)
        Path(output).write_text(header, encoding="utf-8")
        rprint(f"[green]Written {output}[/green]")
    except Exception as e:
        rprint(f"[red]Failed to generate header: {e}[/red]")
        raise typer.Exit(1)


@app.command()
def raw(
    data: str = typer.Argument(help="Hex string of ESC/POS bytes, e.g. '1b40' for reset"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Send raw ESC/POS bytes (hex-encoded)."""
    import base64
    raw_bytes = bytes.fromhex(data)
    b64 = base64.b64encode(raw_bytes).decode()
    get_client(printer).print_raw(b64)
    rprint(f"[green]Sent {len(raw_bytes)} bytes.[/green]")


@app.command()
def feed(
    lines: int = typer.Option(3, "--lines", "-n"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Feed paper."""
    get_client(printer).feed(lines)


@app.command()
def cut(printer: PrinterURL = DEFAULT_BASE_URL):
    """Cut paper."""
    get_client(printer).cut()


@app.command()
def update(
    firmware: str = typer.Argument(help="Path to firmware .bin file"),
    printer: PrinterURL = DEFAULT_BASE_URL,
):
    """Upload firmware over-the-air."""
    from pathlib import Path

    path = Path(firmware)
    if not path.exists():
        rprint(f"[red]File not found: {firmware}[/red]")
        raise typer.Exit(1)
    if not path.suffix == ".bin":
        rprint(f"[yellow]Warning: expected .bin file, got {path.suffix}[/yellow]")

    size_kb = path.stat().st_size / 1024
    rprint(f"Uploading {path.name} ({size_kb:.0f} KB)...")
    try:
        result = get_client(printer).update(str(path))
        rprint(f"[green]{result}[/green]")
        rprint("Printer is rebooting — wait a few seconds, then run [bold]rp status[/bold].")
    except Exception as e:
        rprint(f"[red]Update failed: {e}[/red]")
        raise typer.Exit(1)


if __name__ == "__main__":
    app()

from .client import PrinterClient


def print_note(client: PrinterClient, title: str, body: str):
    """Print a formatted note with title and body text."""
    # Title in bold, centered
    client.print_text(title, bold=True, align=1, font_width=2, font_height=2)
    # Separator
    client.print_text("-" * 32)
    # Body
    client.print_text(body)


def print_todo(client: PrinterClient, title: str, items: list[str]):
    """Print a checklist with empty checkbox squares."""
    # Title
    client.print_text(title, bold=True, font_width=2, font_height=1)
    client.print_text("")
    # Items with checkbox
    for item in items:
        client.print_text(f"[ ] {item}")
    client.print_text("")

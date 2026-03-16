import httpx

DEFAULT_BASE_URL = "http://printer.local"
TIMEOUT = 10.0


class PrinterClient:
    def __init__(self, base_url: str = DEFAULT_BASE_URL):
        self.base_url = base_url.rstrip("/")

    def _url(self, path: str) -> str:
        return f"{self.base_url}{path}"

    def status(self) -> dict:
        r = httpx.get(self._url("/status"), timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def print_text(self, text: str, bold: bool = False, underline: bool = False,
                   align: int = 0, font_width: int = 1, font_height: int = 1) -> dict:
        r = httpx.post(self._url("/print/text"), json={
            "text": text,
            "bold": bold,
            "underline": underline,
            "align": align,
            "font_width": font_width,
            "font_height": font_height,
        }, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def print_receipt(self, title: str, items: list[dict], total: str = "",
                      footer: str = "") -> dict:
        r = httpx.post(self._url("/print/receipt"), json={
            "title": title,
            "items": items,
            "total": total,
            "footer": footer,
        }, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def print_image(self, data_b64: str, width: int, height: int) -> dict:
        r = httpx.post(self._url("/print/image"), json={
            "data": data_b64,
            "width": width,
            "height": height,
        }, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def print_qrcode(self, data: str, label: str = "", size: int = 4) -> dict:
        r = httpx.post(self._url("/print/qrcode"), json={
            "data": data,
            "label": label,
            "size": size,
        }, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def submit(self, message: str, date: str = "") -> str:
        r = httpx.post(self._url("/submit"), data={
            "message": message,
            "date": date,
        }, timeout=TIMEOUT)
        r.raise_for_status()
        return r.text

    def feed(self, lines: int = 3) -> dict:
        r = httpx.post(self._url("/feed"), data={"lines": str(lines)}, timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

    def cut(self) -> dict:
        r = httpx.post(self._url("/cut"), timeout=TIMEOUT)
        r.raise_for_status()
        return r.json()

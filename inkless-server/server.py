"""inkless-server: AI proxy for the Inkless receipt printer."""

import json
import logging
import os
from contextlib import asynccontextmanager
from datetime import datetime
from typing import Optional

import anthropic
import httpx
import uvicorn
from dotenv import load_dotenv
from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse
from pydantic import BaseModel

load_dotenv()

log = logging.getLogger("inkless-server")

ANTHROPIC_API_KEY = os.environ.get("ANTHROPIC_API_KEY", "")
PRINTER_URL = os.environ.get("PRINTER_URL", "http://printer.local")

# Reusable async client (connection pooling across requests)
ai_client = anthropic.AsyncAnthropic(api_key=ANTHROPIC_API_KEY) if ANTHROPIC_API_KEY else None


@asynccontextmanager
async def lifespan(app: FastAPI):
    if not ANTHROPIC_API_KEY:
        log.warning("ANTHROPIC_API_KEY not set")
    log.info("Printer: %s", PRINTER_URL)
    log.info("Ready on http://localhost:8100")
    yield


app = FastAPI(title="inkless-server", lifespan=lifespan)

# Allow requests from the ESP32 web UI (any origin on local network)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/status")
async def status():
    return {
        "status": "online",
        "printer_url": PRINTER_URL,
        "api_key_set": bool(ANTHROPIC_API_KEY),
    }


@app.post("/api/generate")
async def generate(request: Request):
    """Proxy AI requests to Anthropic. Supports both streaming and non-streaming."""
    if not ai_client:
        return JSONResponse(
            status_code=503,
            content={"error": {"message": "ANTHROPIC_API_KEY not configured"}},
        )

    try:
        body = await request.json()
    except Exception:
        return JSONResponse(
            status_code=400,
            content={"error": {"message": "Invalid JSON body"}},
        )

    model = body.get("model", "claude-haiku-4-5-20251001")
    max_tokens = body.get("max_tokens", 500)
    system = body.get("system", "")
    messages = body.get("messages", [])
    stream = body.get("stream", False)
    tools = body.get("tools", None)

    if stream:
        async def event_stream():
            try:
                async with ai_client.messages.stream(
                    model=model,
                    max_tokens=max_tokens,
                    system=system,
                    messages=messages,
                    tools=tools if tools is not None else anthropic.NOT_GIVEN,
                ) as stream_resp:
                    async for event in stream_resp:
                        yield f"data: {event.model_dump_json()}\n\n"
            except Exception as e:
                log.exception("Streaming error")
                yield f"data: {json.dumps({'type': 'error', 'error': {'message': str(e)}})}\n\n"

        return StreamingResponse(
            event_stream(),
            media_type="text/event-stream",
            headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
        )
    else:
        try:
            response = await ai_client.messages.create(
                model=model,
                max_tokens=max_tokens,
                system=system,
                messages=messages,
                tools=tools if tools is not None else anthropic.NOT_GIVEN,
            )
            return response.model_dump()
        except anthropic.RateLimitError as e:
            return JSONResponse(status_code=429, content={"error": {"message": str(e)}})
        except anthropic.AuthenticationError as e:
            return JSONResponse(status_code=401, content={"error": {"message": str(e)}})
        except Exception as e:
            log.exception("Generate error")
            return JSONResponse(status_code=500, content={"error": {"message": str(e)}})


# --- Printer helpers ---

import re
import unicodedata

def sanitize(text: str) -> str:
    """Replace Unicode characters with ASCII equivalents for the thermal printer."""
    # Smart quotes
    text = re.sub(r'[\u2018\u2019\u201A\u2032]', "'", text)
    text = re.sub(r'[\u201C\u201D\u201E\u2033]', '"', text)
    # Dashes
    text = re.sub(r'[\u2013\u2014]', '-', text)
    # Ellipsis
    text = text.replace('\u2026', '...')
    # Spaces
    text = re.sub(r'[\u00A0\u2002\u2003\u2009]', ' ', text)
    # Bullets
    text = text.replace('\u2022', '-')
    # Strip accents: é→e, ü→u, etc.
    nfkd = unicodedata.normalize('NFKD', text)
    text = ''.join(c for c in nfkd if not unicodedata.combining(c))
    # Drop any remaining non-ASCII
    text = re.sub(r'[^\x00-\x7F]', '', text)
    return text


async def print_text(text: str):
    """Send sanitized text to the printer via its HTTP API."""
    async with httpx.AsyncClient() as client:
        r = await client.post(f"{PRINTER_URL}/print/text", json={"text": sanitize(text)}, timeout=10.0)
        r.raise_for_status()


async def print_logo():
    """Print the logo on the printer."""
    async with httpx.AsyncClient() as client:
        r = await client.post(f"{PRINTER_URL}/print/logo/raster", timeout=30.0)
        r.raise_for_status()


async def print_qrcode(data: str, size: int = 4):
    """Print a QR code on the printer."""
    async with httpx.AsyncClient() as client:
        r = await client.post(
            f"{PRINTER_URL}/print/qrcode",
            json={"data": data, "size": size},
            timeout=10.0,
        )
        r.raise_for_status()


class PrintRequest(BaseModel):
    text: str
    logo: bool = False
    qr: Optional[str] = None


@app.post("/api/print")
async def print_receipt(req: PrintRequest):
    """Print text (with optional logo and QR). Designed for iOS Shortcuts / automations."""
    if req.logo:
        await print_logo()
    await print_text(req.text)
    if req.qr:
        await print_qrcode(req.qr)
    return {"status": "printed"}


class DayEndRequest(BaseModel):
    progress: str


DAYEND_SYSTEM = (
    "You are a receipt printer at Strebergarten.studio, a design and product "
    "studio. You print end-of-day shutdown receipts — a small ritual to mark "
    "the transition from work to rest. Maximum 32 characters per line. No "
    "markdown. Just the content, nothing else."
)

DAYEND_PROMPT = """Someone is about to leave the studio for the day. Here's what they said they made progress on:

"{progress}"

Write them a short, warm shutdown receipt. Format:

CLOSING TIME
strebergarten.studio
--------------------------------
Today you:
(restate what they did in 2-3
crisp bullet points, each
starting with a verb)
--------------------------------
(a short, poetic 2-3 line
reflection — something that
honors the work without being
cheesy. Think: grounding, not
motivational poster.)
--------------------------------
Tomorrow is another plot.
Leave this here. Go home.

Keep it to 32 chars per line max. Be warm, grounded, honest. No markdown."""


@app.post("/api/print/dayend")
async def print_dayend(req: DayEndRequest):
    """Generate and print an end-of-day shutdown receipt."""
    if not ai_client:
        return JSONResponse(status_code=503, content={"error": "API key not configured"})

    now = datetime.now()
    date_str = now.strftime("%A, %d %B %Y")
    time_str = now.strftime("%H:%M")

    prompt = DAYEND_PROMPT.format(progress=req.progress)

    try:
        response = await ai_client.messages.create(
            model="claude-haiku-4-5-20251001",
            max_tokens=500,
            system=DAYEND_SYSTEM,
            messages=[{"role": "user", "content": prompt}],
        )
    except anthropic.RateLimitError as e:
        return JSONResponse(status_code=429, content={"error": {"message": str(e)}})
    except anthropic.AuthenticationError as e:
        return JSONResponse(status_code=401, content={"error": {"message": str(e)}})
    except Exception as e:
        log.exception("Day end generation error")
        return JSONResponse(status_code=500, content={"error": {"message": str(e)}})

    if not response.content or not hasattr(response.content[0], "text"):
        return JSONResponse(status_code=500, content={"error": {"message": "No text in AI response"}})

    receipt_body = response.content[0].text
    header = f"{date_str}, {time_str}\n"
    full_receipt = header + receipt_body

    try:
        await print_logo()
        await print_text(full_receipt)
        # Extra feed so the receipt can be torn off
        async with httpx.AsyncClient() as client:
            await client.post(f"{PRINTER_URL}/feed", data={"lines": "6"}, timeout=10.0)
    except httpx.HTTPError as e:
        log.exception("Printer communication error")
        return JSONResponse(
            status_code=502,
            content={"error": {"message": f"Printer unavailable: {e}"}, "text": full_receipt},
        )

    return {"status": "printed", "text": full_receipt}


def main():
    reload = os.environ.get("INKLESS_DEV", "").lower() in ("1", "true")
    uvicorn.run("server:app", host="0.0.0.0", port=8100, reload=reload)


if __name__ == "__main__":
    main()

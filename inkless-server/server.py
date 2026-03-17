"""inkless-server: AI proxy for the Inkless receipt printer."""

import os
from contextlib import asynccontextmanager

import anthropic
import uvicorn
from dotenv import load_dotenv
from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, StreamingResponse

load_dotenv()

ANTHROPIC_API_KEY = os.environ.get("ANTHROPIC_API_KEY", "")
PRINTER_URL = os.environ.get("PRINTER_URL", "http://printer.local")

# Reusable async client (connection pooling across requests)
ai_client = anthropic.AsyncAnthropic(api_key=ANTHROPIC_API_KEY) if ANTHROPIC_API_KEY else None


@asynccontextmanager
async def lifespan(app: FastAPI):
    if not ANTHROPIC_API_KEY:
        print("[inkless-server] WARNING: ANTHROPIC_API_KEY not set")
    print(f"[inkless-server] Printer: {PRINTER_URL}")
    print(f"[inkless-server] Ready on http://localhost:8100")
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
    if not ANTHROPIC_API_KEY:
        return JSONResponse(
            status_code=503,
            content={"error": {"message": "ANTHROPIC_API_KEY not configured"}},
        )

    body = await request.json()

    model = body.get("model", "claude-haiku-4-5-20251001")
    max_tokens = body.get("max_tokens", 500)
    system = body.get("system", "")
    messages = body.get("messages", [])
    stream = body.get("stream", False)
    tools = body.get("tools", None)

    if stream:
        # Streaming: proxy SSE events back to the browser
        async def event_stream():
            async with ai_client.messages.stream(
                model=model,
                max_tokens=max_tokens,
                system=system,
                messages=messages,
                tools=tools if tools is not None else anthropic.NOT_GIVEN,
            ) as stream_resp:
                async for event in stream_resp:
                    yield f"data: {event.model_dump_json()}\n\n"

        return StreamingResponse(
            event_stream(),
            media_type="text/event-stream",
            headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
        )
    else:
        # Non-streaming: return full response
        response = await ai_client.messages.create(
            model=model,
            max_tokens=max_tokens,
            system=system,
            messages=messages,
            tools=tools if tools is not None else anthropic.NOT_GIVEN,
        )
        return response.model_dump()


def main():
    uvicorn.run("server:app", host="0.0.0.0", port=8100, reload=True)


if __name__ == "__main__":
    main()

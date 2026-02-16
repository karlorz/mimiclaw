#!/usr/bin/env python3
import argparse
import asyncio
import json
import sys

try:
    import websockets
except Exception as exc:  # pragma: no cover
    print(f"missing dependency: websockets ({exc})", file=sys.stderr)
    sys.exit(2)


FALLBACK_ERROR = "Sorry, I encountered an error."
WORKING_STATUS_MESSAGES = {
    "mimi😗is working...",
    "mimi🐾 is thinking...",
    "mimi💭 is pondering...",
    "mimi🌙 is on it...",
    "mimi✨ is cooking...",
}
WORKING_STATUS_HINTS = (
    "is working...",
    "is thinking...",
    "is pondering...",
    "is on it...",
    "is cooking...",
)


class SmokeFailure(Exception):
    pass


def is_working_status(text: str) -> bool:
    if text in WORKING_STATUS_MESSAGES:
        return True

    normalized = text.lower()
    if not normalized.startswith("mimi"):
        return False
    return any(hint in normalized for hint in WORKING_STATUS_HINTS)


async def expect_live_response(
    ws: "websockets.WebSocketClientProtocol",
    chat_id: str,
    timeout_s: float,
    disallow_fallback: str,
) -> None:
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout_s

    while True:
        remaining = deadline - loop.time()
        if remaining <= 0:
            raise SmokeFailure(f"timeout waiting for response ({timeout_s:.1f}s)")

        raw = await asyncio.wait_for(ws.recv(), timeout=remaining)

        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            continue

        if not isinstance(msg, dict):
            continue
        if msg.get("type") != "response":
            continue
        if msg.get("chat_id") != chat_id:
            continue

        text = msg.get("content")
        if not isinstance(text, str):
            raise SmokeFailure("invalid response content type")

        text = text.strip()
        if not text:
            raise SmokeFailure("empty response content")

        if is_working_status(text):
            continue

        if disallow_fallback and text == disallow_fallback.strip():
            raise SmokeFailure(
                "received disallowed fallback response; likely missing API key or provider failure"
            )

        print(json.dumps(msg, ensure_ascii=False))
        return


async def run_once(
    url: str,
    chat_id: str,
    content: str,
    timeout_s: float,
    disallow_fallback: str,
) -> None:
    payload = {"type": "message", "chat_id": chat_id, "content": content}

    async with websockets.connect(
        url,
        ping_interval=None,
        close_timeout=1.0,
        open_timeout=timeout_s,
    ) as ws:
        await ws.send(json.dumps(payload))
        await expect_live_response(ws, chat_id, timeout_s, disallow_fallback)


async def run(
    url: str,
    chat_id: str,
    content: str,
    timeout_s: float,
    disallow_fallback: str,
) -> int:
    # Retry once for transient network/provider hiccups.
    max_attempts = 2
    last_error = "unknown error"

    for attempt in range(1, max_attempts + 1):
        try:
            await run_once(url, chat_id, content, timeout_s, disallow_fallback)
            if attempt > 1:
                print("live smoke passed on retry", file=sys.stderr)
            return 0
        except SmokeFailure as exc:
            last_error = str(exc)
        except Exception as exc:
            last_error = f"{type(exc).__name__}: {exc}"

        if attempt < max_attempts:
            print(f"attempt {attempt} failed: {last_error}; retrying once...", file=sys.stderr)
            await asyncio.sleep(0.5)

    print(f"live smoke failed after {max_attempts} attempts: {last_error}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="MimiClaw host websocket live validation client"
    )
    parser.add_argument("--url", required=True)
    parser.add_argument("--chat-id", default="ci_live")
    parser.add_argument("--content", default="Reply with a short live-check acknowledgement.")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--disallow-fallback", default=FALLBACK_ERROR)
    args = parser.parse_args()

    try:
        return asyncio.run(
            run(
                args.url,
                args.chat_id,
                args.content,
                args.timeout,
                args.disallow_fallback,
            )
        )
    except Exception as exc:
        print(f"live smoke ws failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

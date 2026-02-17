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

WORKING_STATUS_HINTS = (
    "is working...",
    "is thinking...",
    "is pondering...",
    "is on it...",
    "is cooking...",
)


def is_working_status(text: str) -> bool:
    normalized = text.lower()
    if not normalized.startswith("mimi"):
        return False
    return any(hint in normalized for hint in WORKING_STATUS_HINTS)


async def run(url: str, chat_id: str, content: str, timeout_s: float) -> int:
    payload = {"type": "message", "chat_id": chat_id, "content": content}

    async with websockets.connect(
        url, ping_interval=None, close_timeout=1.0, open_timeout=timeout_s
    ) as ws:
        await ws.send(json.dumps(payload))
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout_s

        while True:
            remaining = deadline - loop.time()
            if remaining <= 0:
                print(f"timeout waiting for response ({timeout_s:.1f}s)", file=sys.stderr)
                return 1
            try:
                raw = await asyncio.wait_for(ws.recv(), timeout=remaining)
            except asyncio.TimeoutError:
                print(f"timeout waiting for response ({timeout_s:.1f}s)", file=sys.stderr)
                return 1
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
                print("invalid response content type", file=sys.stderr)
                return 1
            text = text.strip()
            if not text:
                print("empty response content", file=sys.stderr)
                return 1

            if is_working_status(text):
                continue

            print(json.dumps(msg, ensure_ascii=False))
            return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="MimiClaw host websocket smoke client")
    parser.add_argument("--url", required=True)
    parser.add_argument("--chat-id", default="ci_smoke")
    parser.add_argument("--content", default="ping")
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()

    try:
        return asyncio.run(run(args.url, args.chat_id, args.content, args.timeout))
    except Exception as exc:
        print(f"smoke ws failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

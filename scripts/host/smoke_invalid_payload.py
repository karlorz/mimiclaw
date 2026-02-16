#!/usr/bin/env python3
import argparse
import asyncio
import json
import os
import sys

try:
    import websockets
    from websockets.exceptions import ConnectionClosed
except Exception as exc:  # pragma: no cover
    print(f"missing dependency: websockets ({exc})", file=sys.stderr)
    sys.exit(2)


def check_pid_alive(pid: int) -> bool:
    if pid <= 0:
        return True
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


async def send_invalid_frames(url: str, timeout_s: float) -> None:
    try:
        async with websockets.connect(
            url,
            ping_interval=None,
            close_timeout=1.0,
            open_timeout=timeout_s,
        ) as ws:
            # malformed JSON payload
            await ws.send('{"type":"message"')

            # valid JSON but not a message frame
            await ws.send(
                json.dumps(
                    {
                        "type": "not_message",
                        "chat_id": "ci_invalid",
                        "content": "ignored",
                    }
                )
            )

            # non-text frame
            await ws.send(b"\x00\x01\x02\x03")
            await asyncio.sleep(0.25)
    except ConnectionClosed:
        # If the server closes this connection, that's acceptable as long as
        # the daemon remains alive and accepts subsequent connections.
        return


async def probe_new_connection(url: str, timeout_s: float) -> None:
    async with websockets.connect(
        url,
        ping_interval=None,
        close_timeout=1.0,
        open_timeout=timeout_s,
    ) as ws:
        await ws.send(json.dumps({"type": "probe"}))
        await asyncio.sleep(0.1)


async def run(url: str, pid: int, timeout_s: float) -> int:
    if not check_pid_alive(pid):
        print("host process is not running before robustness check", file=sys.stderr)
        return 1

    await send_invalid_frames(url, timeout_s)
    await asyncio.sleep(0.25)

    if not check_pid_alive(pid):
        print("host process exited after invalid payloads", file=sys.stderr)
        return 1

    await probe_new_connection(url, timeout_s)

    if not check_pid_alive(pid):
        print("host process exited after reconnect probe", file=sys.stderr)
        return 1

    print("invalid payload robustness check passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send invalid websocket frames and assert host daemon stays alive"
    )
    parser.add_argument("--url", required=True)
    parser.add_argument("--pid", type=int, default=0)
    parser.add_argument("--timeout", type=float, default=10.0)
    args = parser.parse_args()

    try:
        return asyncio.run(run(args.url, args.pid, args.timeout))
    except Exception as exc:
        print(f"invalid payload smoke failed: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

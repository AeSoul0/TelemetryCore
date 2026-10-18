#!/usr/bin/env python3
"""TelemetryCore ground station.

Reads the frames sent by the firmware (or by the PC simulator), validates them and shows a live
dashboard in the terminal.

Examples:
    # from the NUCLEO board (ST-LINK virtual COM port)
    python ground_station.py --port COM5
    python ground_station.py --port /dev/ttyACM0 --csv telemetry.csv

    # from the PC simulator
    telemetry_sim --forever | python ground_station.py --stdin

    # CI: consume everything, print a summary, fail on corrupted or lost frames
    telemetry_sim --duration-ms 3000 | python ground_station.py --stdin --check
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
import time
from collections import deque
from typing import IO, Deque, Iterator, List, Optional

from telemetry_protocol import (
    Boot,
    LinkStats,
    LogRecord,
    PayloadError,
    FrameError,
    Telemetry,
    parse_line,
)

SPARK_CHARS = "▁▂▃▄▅▆▇█"
HISTORY = 60
LOG_LINES = 8

COLORS = {
    "OK": "\x1b[32m",
    "WARNING": "\x1b[33m",
    "CRITICAL": "\x1b[31m",
    "WARN": "\x1b[33m",
    "ERROR": "\x1b[31m",
    "DEBUG": "\x1b[2m",
    "INFO": "",
}
RESET = "\x1b[0m"


# --- Input sources ------------------------------------------------------------------------------


def read_stream(stream: IO[bytes]) -> Iterator[bytes]:
    for line in stream:
        yield line


def read_serial(port: str, baud: int) -> Iterator[bytes]:
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        sys.exit("pyserial is required for --port: pip install -r requirements.txt")
    with serial.Serial(port, baud, timeout=1) as link:
        while True:
            line = link.readline()
            if line:
                yield line


# --- Presentation -------------------------------------------------------------------------------


def sparkline(values: Deque[float]) -> str:
    if not values:
        return ""
    low, high = min(values), max(values)
    span = (high - low) or 1.0
    return "".join(SPARK_CHARS[int((v - low) / span * (len(SPARK_CHARS) - 1))] for v in values)


def battery_bar(percent: int, width: int = 20) -> str:
    filled = max(0, min(width, round(percent / 100 * width)))
    return "█" * filled + "░" * (width - filled)


def format_uptime(ms: int) -> str:
    seconds, millis = divmod(ms, 1000)
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}.{millis // 100}"


class Dashboard:
    def __init__(self, source: str, color: bool) -> None:
        self.source = source
        self.color = color
        self.boot: Optional[Boot] = None
        self.last: Optional[Telemetry] = None
        self.temperatures: Deque[float] = deque(maxlen=HISTORY)
        self.logs: Deque[LogRecord] = deque(maxlen=LOG_LINES)
        self.arrivals: Deque[float] = deque(maxlen=50)
        self.last_draw = 0.0

    def paint(self, text: str, key: str) -> str:
        code = COLORS.get(key, "")
        return f"{code}{text}{RESET}" if self.color and code else text

    def update(self, message: object) -> None:
        if isinstance(message, Telemetry):
            self.last = message
            self.temperatures.append(message.temperature_c)
            self.arrivals.append(time.monotonic())
        elif isinstance(message, LogRecord):
            self.logs.append(message)
        elif isinstance(message, Boot):
            self.boot = message
            self.temperatures.clear()

    def rate_hz(self) -> float:
        if len(self.arrivals) < 2:
            return 0.0
        elapsed = self.arrivals[-1] - self.arrivals[0]
        return (len(self.arrivals) - 1) / elapsed if elapsed > 0 else 0.0

    def render(self, stats: LinkStats) -> str:
        rule = "─" * 72
        boot = f"fw {self.boot.firmware} · board {self.boot.board}" if self.boot else "waiting for BOOT frame"
        lines: List[str] = [f" TelemetryCore Ground Station   source: {self.source}   {boot}", rule]
        t = self.last
        if t is None:
            lines.append(" waiting for telemetry...")
        else:
            faults = ", ".join(t.faults) or "none"
            lines += [
                f" Status       {self.paint(f'{t.status:<10}', t.status)}  Uptime  {format_uptime(t.uptime_ms)}"
                f"   seq {t.seq}",
                f" Temperature  {t.temperature_c:6.1f} °C    {sparkline(self.temperatures)}",
                f" Pressure     {t.pressure_hpa:7.1f} hPa",
                f" Battery      {t.battery_pct:4d} %     {battery_bar(t.battery_pct)}",
                f" Faults       {self.paint(faults, 'OK' if faults == 'none' else 'WARNING')}",
            ]
        lines += [
            rule,
            f" Link: {stats.frames_ok} frames · {stats.checksum_errors} checksum errors · "
            f"{stats.malformed} malformed · {stats.lost} lost · {self.rate_hz():4.1f} Hz telemetry",
            " Last logs:",
        ]
        for record in self.logs:
            lines.append(f"  {self.paint(f'{record.level:<5}', record.level)} {record.component:<11} {record.message}")
        return "\n".join(lines)

    def draw(self, stats: LinkStats, force: bool = False) -> None:
        now = time.monotonic()
        if not force and now - self.last_draw < 0.2:
            return
        self.last_draw = now
        sys.stdout.write("\x1b[H\x1b[2J" + self.render(stats) + "\n")
        sys.stdout.flush()


def plain_line(message: object) -> Optional[str]:
    if isinstance(message, Telemetry):
        faults = ",".join(message.faults) or "-"
        return (
            f"[{message.uptime_ms:>8} ms] #{message.seq:<5} T={message.temperature_c:5.1f}°C "
            f"P={message.pressure_hpa:6.1f}hPa B={message.battery_pct:3d}% {message.status:<8} faults={faults}"
        )
    if isinstance(message, LogRecord):
        return f"{'':13}{message.level:<5} {message.component}: {message.message}"
    if isinstance(message, Boot):
        return f"BOOT fw={message.firmware} board={message.board}"
    return None


def summary(stats: LinkStats) -> str:
    levels = ", ".join(f"{k}={v}" for k, v in sorted(stats.log_levels.items())) or "none"
    return (
        f"frames={stats.frames_ok} telemetry={stats.telemetry} logs={stats.logs} ({levels}) boots={stats.boots} "
        f"checksum_errors={stats.checksum_errors} malformed={stats.malformed} lost={stats.lost}"
    )


# --- Main loop ----------------------------------------------------------------------------------


def run(lines: Iterator[bytes], args: argparse.Namespace, source: str) -> int:
    stats = LinkStats()
    color = not args.no_color and sys.stdout.isatty()
    dashboard = Dashboard(source, color) if not (args.plain or args.check) else None
    if dashboard and os.name == "nt":
        os.system("")  # enables ANSI escape sequences in the Windows console

    csv_file = open(args.csv, "w", newline="", encoding="utf-8") if args.csv else None
    writer = csv.writer(csv_file) if csv_file else None
    if writer:
        writer.writerow(["seq", "uptime_ms", "temperature_c", "pressure_hpa", "battery_pct", "status", "fault_flags"])

    try:
        for raw in lines:
            if not raw.strip():
                continue
            try:
                message = parse_line(raw)
            except (FrameError, PayloadError) as error:
                stats.record_error(error)
                if args.plain or args.check:
                    print(f"!! {error}: {raw!r}", file=sys.stderr)
                continue
            stats.record(message)
            if writer and isinstance(message, Telemetry):
                writer.writerow(
                    [message.seq, message.uptime_ms, message.temperature_c, message.pressure_hpa,
                     message.battery_pct, message.status, f"{message.fault_flags:02X}"]
                )
            if dashboard:
                dashboard.update(message)
                dashboard.draw(stats)
            elif args.plain:
                text = plain_line(message)
                if text:
                    print(text, flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        if csv_file:
            csv_file.close()

    if dashboard:
        dashboard.draw(stats, force=True)
    print(summary(stats), file=sys.stderr if not args.check else sys.stdout)
    if args.check and not stats.healthy:
        print("CHECK FAILED", file=sys.stderr)
        return 1
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="serial port (e.g. COM5, /dev/ttyACM0)")
    source.add_argument("--stdin", action="store_true", help="read frames from standard input")
    source.add_argument("--replay", metavar="FILE", help="read frames from a capture file")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--plain", action="store_true", help="one line per frame instead of the dashboard")
    parser.add_argument("--check", action="store_true", help="no output; exit 1 on corrupted/lost frames or no telemetry")
    parser.add_argument("--csv", metavar="FILE", help="also log telemetry to a CSV file")
    parser.add_argument("--no-color", action="store_true")
    args = parser.parse_args(argv)

    # Pipes default to the locale encoding on Windows (cp1252): force UTF-8 for "°C" and the bars.
    if not sys.stdout.isatty() and hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    if args.port:
        return run(read_serial(args.port, args.baud), args, args.port)
    if args.replay:
        with open(args.replay, "rb") as capture:
            return run(read_stream(capture), args, args.replay)
    return run(read_stream(sys.stdin.buffer), args, "stdin")


if __name__ == "__main__":
    sys.exit(main())

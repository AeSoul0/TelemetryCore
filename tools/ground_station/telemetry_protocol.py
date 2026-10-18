"""TelemetryCore serial protocol: frame decoding, parsing and link statistics.

Frame format (see docs/protocol.md):

    $<payload>*<XOR8 checksum as 2 hex digits>\\r\\n

Payload types:
    BOOT,fw=<version>,board=<name>
    TLM,<seq>,<uptime_ms>,<temp_C>,<press_hPa>,<batt_pct>,<status>,<fault_flags_hex>
    LOG,<LEVEL>,<component>,<message>      (the message may contain commas)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Union

FAULT_FLAG_NAMES = {
    0x01: "SENSOR_ERROR",
    0x02: "INVALID_DATA",
    0x04: "QUEUE_OVERFLOW",
    0x08: "BATTERY_LOW",
}

STATUSES = ("OK", "WARNING", "CRITICAL")


class FrameError(ValueError):
    """The line is not a valid frame (missing delimiters or bad checksum)."""


class PayloadError(ValueError):
    """The frame is valid but its payload cannot be interpreted."""


@dataclass(frozen=True)
class Boot:
    fields: Dict[str, str]

    @property
    def firmware(self) -> str:
        return self.fields.get("fw", "?")

    @property
    def board(self) -> str:
        return self.fields.get("board", "?")


@dataclass(frozen=True)
class Telemetry:
    seq: int
    uptime_ms: int
    temperature_c: float
    pressure_hpa: float
    battery_pct: int
    status: str
    fault_flags: int

    @property
    def faults(self) -> List[str]:
        return describe_flags(self.fault_flags)


@dataclass(frozen=True)
class LogRecord:
    level: str
    component: str
    message: str


@dataclass(frozen=True)
class Unknown:
    kind: str
    payload: str


Message = Union[Boot, Telemetry, LogRecord, Unknown]


def checksum(payload: str) -> int:
    value = 0
    for byte in payload.encode("ascii", errors="replace"):
        value ^= byte
    return value


def encode(payload: str) -> str:
    """Builds a frame, e.g. for injecting commands or writing test fixtures."""
    return f"${payload}*{checksum(payload):02X}\r\n"


def decode(line: Union[str, bytes]) -> str:
    """Validates a frame and returns its payload. Raises FrameError."""
    if isinstance(line, bytes):
        line = line.decode("ascii", errors="replace")
    line = line.strip()
    if not line.startswith("$"):
        raise FrameError("missing '$'")
    star = line.rfind("*")
    if star < 0 or len(line) - star != 3:
        raise FrameError("missing '*HH' checksum")
    payload = line[1:star]
    try:
        expected = int(line[star + 1 :], 16)
    except ValueError as exc:
        raise FrameError("checksum is not hexadecimal") from exc
    actual = checksum(payload)
    if actual != expected:
        raise FrameError(f"checksum mismatch: got {actual:02X}, frame says {expected:02X}")
    return payload


def parse_payload(payload: str) -> Message:
    kind, _, rest = payload.partition(",")
    if kind == "TLM":
        fields = rest.split(",")
        if len(fields) != 7:
            raise PayloadError(f"TLM expects 7 fields, got {len(fields)}")
        try:
            return Telemetry(
                seq=int(fields[0]),
                uptime_ms=int(fields[1]),
                temperature_c=float(fields[2]),
                pressure_hpa=float(fields[3]),
                battery_pct=int(fields[4]),
                status=fields[5],
                fault_flags=int(fields[6], 16),
            )
        except ValueError as exc:
            raise PayloadError(f"bad TLM field: {exc}") from exc
    if kind == "LOG":
        parts = rest.split(",", 2)
        if len(parts) != 3:
            raise PayloadError("LOG expects level, component and message")
        return LogRecord(level=parts[0], component=parts[1], message=parts[2])
    if kind == "BOOT":
        boot_fields: Dict[str, str] = {}
        for item in rest.split(","):
            key, _, value = item.partition("=")
            if key:
                boot_fields[key] = value
        return Boot(fields=boot_fields)
    return Unknown(kind=kind, payload=payload)


def parse_line(line: Union[str, bytes]) -> Message:
    return parse_payload(decode(line))


def describe_flags(flags: int) -> List[str]:
    names = [name for bit, name in FAULT_FLAG_NAMES.items() if flags & bit]
    unknown = flags & ~sum(FAULT_FLAG_NAMES)
    if unknown:
        names.append(f"0x{unknown:02X}")
    return names


@dataclass
class LinkStats:
    """Counts what arrived on the link and detects telemetry lost between two packets."""

    frames_ok: int = 0
    checksum_errors: int = 0
    malformed: int = 0
    telemetry: int = 0
    logs: int = 0
    boots: int = 0
    lost: int = 0
    last_seq: Optional[int] = None
    log_levels: Dict[str, int] = field(default_factory=dict)

    def record_error(self, error: Exception) -> None:
        if isinstance(error, FrameError):
            self.checksum_errors += 1
        else:
            self.malformed += 1

    def record(self, message: Message) -> None:
        self.frames_ok += 1
        if isinstance(message, Telemetry):
            self.telemetry += 1
            if self.last_seq is not None and message.seq > self.last_seq + 1:
                self.lost += message.seq - self.last_seq - 1
            self.last_seq = message.seq
        elif isinstance(message, LogRecord):
            self.logs += 1
            self.log_levels[message.level] = self.log_levels.get(message.level, 0) + 1
        elif isinstance(message, Boot):
            # The board restarted: sequence numbers start again from 0.
            self.boots += 1
            self.last_seq = None

    @property
    def healthy(self) -> bool:
        return self.telemetry > 0 and self.checksum_errors == 0 and self.malformed == 0 and self.lost == 0

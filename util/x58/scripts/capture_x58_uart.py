#!/usr/bin/env python3
"""Capture an X58 UART boot after an observed quiet interval.

The quiet gate is useful when a previous ROMMON session is producing output:
start this program first, remove power, and only the following boot is stored.
The serial port is never written by this program.

By default a bounded token bucket and a consecutive-identical-line detector
stop replay-contaminated captures.  The raw file retains every byte from the
last read which produced the evidence, and the metadata records why it stopped.
"""

from __future__ import annotations

import argparse
import base64
import errno
import fcntl
import hashlib
import json
import os
import select
import sys
import termios
import time
from dataclasses import dataclass, replace
from datetime import datetime, timezone
from pathlib import Path


BAUDS = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}

BITS_PER_8N1_BYTE = 10
DEFAULT_QUEUE_ALLOWANCE = 4096
DEFAULT_REPLAY_BURST_ALLOWANCE = 64 * 1024
DEFAULT_REPLAY_IDENTICAL_LINES = 64
DEFAULT_REPLAY_MIN_LINE_BYTES = 8
DEFAULT_MARKERS = ("x58-dram> ", "x58-usb> ", "rommon> ")


@dataclass(frozen=True)
class CaptureResult:
    total: int
    sha256: str
    matched: bytes | None
    started_monotonic_ns: int
    ended_monotonic_ns: int
    error: str | None
    replay: ReplayEvidence | None = None
    max_identical_lines: int = 0


@dataclass(frozen=True)
class ReplayEvidence:
    kind: str
    total_bytes: int
    monotonic_ns: int
    burst_observed_bytes: int | None = None
    burst_allowed_bytes: float | None = None
    identical_lines: int | None = None
    identical_bytes: int | None = None
    line_sha256: str | None = None
    line_preview_base64: str | None = None


@dataclass(frozen=True)
class RateAssessment:
    elapsed_seconds: float
    observed_bytes_per_second: float
    nominal_max_bytes_per_second: float
    allowed_bytes: float
    plausible: bool


class ReplayGuard:
    """Bound live input bursts and pathological consecutive line replay.

    The token bucket has a finite initial capacity so an already queued, real
    boot suffix can be consumed without being called physically impossible.
    It cannot accumulate unbounded credit during a quiet period.  The line
    detector only considers complete, sufficiently long, immediately repeated
    lines; normal reset phases may repeat earlier log content non-consecutively.
    """

    def __init__(
        self,
        baud: int,
        burst_allowance: int,
        identical_line_limit: int,
        minimum_line_bytes: int,
        started_monotonic_ns: int,
    ) -> None:
        self._bytes_per_second = baud / BITS_PER_8N1_BYTE
        self._capacity = float(burst_allowance)
        self._tokens = float(burst_allowance)
        self._last_monotonic_ns = started_monotonic_ns
        self._identical_line_limit = identical_line_limit
        self._minimum_line_bytes = minimum_line_bytes
        self._pending = bytearray()
        self._last_line: bytes | None = None
        self._identical_lines = 0
        self.max_identical_lines = 0

    def _observe_complete_lines(
        self, chunk: bytes, total_bytes: int, monotonic_ns: int
    ) -> ReplayEvidence | None:
        self._pending.extend(chunk)
        while True:
            newline = self._pending.find(b"\n")
            if newline < 0:
                return None
            line = bytes(self._pending[: newline + 1])
            del self._pending[: newline + 1]

            if line == self._last_line:
                self._identical_lines += 1
            else:
                self._last_line = line
                self._identical_lines = 1
            self.max_identical_lines = max(
                self.max_identical_lines, self._identical_lines
            )

            if (
                len(line) >= self._minimum_line_bytes
                and self._identical_lines >= self._identical_line_limit
            ):
                return ReplayEvidence(
                    kind="identical-line-run",
                    total_bytes=total_bytes,
                    monotonic_ns=monotonic_ns,
                    identical_lines=self._identical_lines,
                    identical_bytes=self._identical_lines * len(line),
                    line_sha256=hashlib.sha256(line).hexdigest(),
                    line_preview_base64=base64.b64encode(line[:128]).decode(
                        "ascii"
                    ),
                )

    def observe(
        self, chunk: bytes, total_bytes: int, monotonic_ns: int
    ) -> ReplayEvidence | None:
        elapsed = max(
            0.0,
            (monotonic_ns - self._last_monotonic_ns) / 1_000_000_000,
        )
        self._tokens = min(
            self._capacity,
            self._tokens + elapsed * self._bytes_per_second,
        )
        self._last_monotonic_ns = monotonic_ns
        allowed = self._tokens
        burst_violation = len(chunk) > allowed + 1e-9
        self._tokens = max(0.0, self._tokens - len(chunk))

        line_evidence = self._observe_complete_lines(
            chunk, total_bytes, monotonic_ns
        )
        if line_evidence is not None:
            if burst_violation:
                return replace(
                    line_evidence,
                    burst_observed_bytes=len(chunk),
                    burst_allowed_bytes=allowed,
                )
            return line_evidence
        if burst_violation:
            return ReplayEvidence(
                kind="burst-rate",
                total_bytes=total_bytes,
                monotonic_ns=monotonic_ns,
                burst_observed_bytes=len(chunk),
                burst_allowed_bytes=allowed,
            )
        return None


def acquire_project_lock(path: Path | None) -> int | None:
    if path is None:
        return None

    flags = os.O_RDWR | os.O_CREAT
    if hasattr(os, "O_CLOEXEC"):
        flags |= os.O_CLOEXEC
    fd = os.open(path, flags, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError as error:
        os.close(fd)
        if error.errno in (errno.EACCES, errno.EAGAIN):
            raise RuntimeError(f"project lock is already held: {path}") from error
        raise
    return fd


def release_project_lock(fd: int | None) -> None:
    if fd is None:
        return
    try:
        fcntl.flock(fd, fcntl.LOCK_UN)
    finally:
        os.close(fd)


def claim_tty_exclusive(fd: int) -> None:
    request = getattr(termios, "TIOCEXCL", None)
    if request is None:
        raise RuntimeError("this platform does not provide TIOCEXCL")
    fcntl.ioctl(fd, request)


def release_tty_exclusive(fd: int) -> None:
    request = getattr(termios, "TIOCNXCL", None)
    if request is not None:
        fcntl.ioctl(fd, request)


def configure(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = BAUDS[baud]
    attrs[5] = BAUDS[baud]
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def read_available(fd: int) -> bytes:
    ready, _, _ = select.select((fd,), (), (), 0.05)
    if not ready:
        return b""
    try:
        return os.read(fd, 65536)
    except BlockingIOError:
        return b""


def wait_for_quiet(fd: int, quiet_seconds: float, deadline: float) -> int:
    last_rx = time.monotonic()
    discarded = 0
    while time.monotonic() < deadline:
        chunk = read_available(fd)
        if chunk:
            discarded += len(chunk)
            last_rx = time.monotonic()
        elif time.monotonic() - last_rx >= quiet_seconds:
            return discarded
    raise TimeoutError("quiet interval was not observed before timeout")


def capture(
    fd: int,
    output: Path,
    markers: tuple[bytes, ...],
    deadline: float,
    marker_linger: float,
    max_bytes: int,
    baud: int,
    replay_policy: str,
    replay_burst_allowance: int,
    replay_identical_lines: int,
    replay_min_line_bytes: int,
) -> CaptureResult:
    digest = hashlib.sha256()
    total = 0
    tail = bytearray()
    matched: bytes | None = None
    matched_at = 0.0
    error: str | None = None
    started_monotonic_ns = time.monotonic_ns()
    replay: ReplayEvidence | None = None
    replay_guard = (
        ReplayGuard(
            baud=baud,
            burst_allowance=replay_burst_allowance,
            identical_line_limit=replay_identical_lines,
            minimum_line_bytes=replay_min_line_bytes,
            started_monotonic_ns=started_monotonic_ns,
        )
        if replay_policy != "off"
        else None
    )

    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    with os.fdopen(os.open(output, flags, 0o600), "wb", buffering=0) as stream:
        while time.monotonic() < deadline:
            chunk = read_available(fd)
            if chunk:
                if total + len(chunk) > max_bytes:
                    error = "capture exceeded the configured byte limit"
                    break
                observed_monotonic_ns = time.monotonic_ns()
                replay_candidate = (
                    replay_guard.observe(
                        chunk,
                        total_bytes=total + len(chunk),
                        monotonic_ns=observed_monotonic_ns,
                    )
                    if replay_guard is not None and replay is None
                    else None
                )
                written = stream.write(chunk)
                if written != len(chunk):
                    raise OSError(
                        f"short capture write: {written} of {len(chunk)} bytes"
                    )
                digest.update(chunk)
                total += len(chunk)
                if replay_candidate is not None:
                    replay = replay_candidate
                    if replay_policy == "fail":
                        error = (
                            "capture stopped by replay guard: "
                            f"{replay.kind}"
                        )
                        break
                tail.extend(chunk)
                if len(tail) > 4096:
                    del tail[:-4096]
                if matched is None:
                    for marker in markers:
                        if marker in tail:
                            matched = marker
                            matched_at = time.monotonic()
                            break
            elif matched is not None and time.monotonic() - matched_at >= marker_linger:
                break
        else:
            error = "capture timeout expired before a terminal marker"
    ended_monotonic_ns = time.monotonic_ns()
    return CaptureResult(
        total=total,
        sha256=digest.hexdigest(),
        matched=matched,
        started_monotonic_ns=started_monotonic_ns,
        ended_monotonic_ns=ended_monotonic_ns,
        error=error,
        replay=replay,
        max_identical_lines=(
            replay_guard.max_identical_lines
            if replay_guard is not None
            else 0
        ),
    )


def assess_rate(
    total: int,
    started_monotonic_ns: int,
    ended_monotonic_ns: int,
    baud: int,
    queue_allowance: int,
) -> RateAssessment:
    elapsed_seconds = max(
        1e-9, (ended_monotonic_ns - started_monotonic_ns) / 1_000_000_000
    )
    nominal_max = baud / BITS_PER_8N1_BYTE
    allowed_bytes = nominal_max * elapsed_seconds + queue_allowance
    observed_rate = total / elapsed_seconds
    return RateAssessment(
        elapsed_seconds=elapsed_seconds,
        observed_bytes_per_second=observed_rate,
        nominal_max_bytes_per_second=nominal_max,
        allowed_bytes=allowed_bytes,
        plausible=total <= allowed_bytes,
    )


def write_json_exclusive(path: Path, value: object) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    with os.fdopen(os.open(path, flags, 0o600), "w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def collapse_identical_lines(
    raw_path: Path, output: Path, raw_sha256: str
) -> dict[str, object]:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    digest = hashlib.sha256()
    line_count = 0
    run_count = 0

    with raw_path.open("rb") as source, os.fdopen(
        os.open(output, flags, 0o600), "wb", buffering=0
    ) as destination:
        header = {
            "schema": "x58-uart-identical-line-rle/v1",
            "artifact": "derived-not-raw-evidence",
            "raw_path": str(raw_path),
            "raw_sha256": raw_sha256,
        }
        encoded = json.dumps(header, sort_keys=True).encode("ascii") + b"\n"
        destination.write(encoded)
        digest.update(encoded)

        previous: bytes | None = None
        count = 0

        def emit(line: bytes, repetitions: int) -> None:
            nonlocal run_count
            record = {
                "count": repetitions,
                "data_base64": base64.b64encode(line).decode("ascii"),
            }
            payload = json.dumps(record, sort_keys=True).encode("ascii") + b"\n"
            destination.write(payload)
            digest.update(payload)
            run_count += 1

        for line in source:
            line_count += 1
            if previous is None:
                previous = line
                count = 1
            elif line == previous:
                count += 1
            else:
                emit(previous, count)
                previous = line
                count = 1
        if previous is not None:
            emit(previous, count)

    return {
        "artifact": "derived-not-raw-evidence",
        "path": str(output),
        "sha256": digest.hexdigest(),
        "source_lines": line_count,
        "runs": run_count,
    }


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--baud", type=int, choices=BAUDS, default=115200)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--quiet", type=float, default=0.5)
    parser.add_argument(
        "--start-immediately",
        action="store_true",
        help=(
            "start without flushing either tty queue or waiting for quiet; "
            "preserves queued input"
        ),
    )
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--marker-linger", type=float, default=0.5)
    parser.add_argument("--max-bytes", type=int, default=16 * 1024 * 1024)
    parser.add_argument(
        "--lock-file",
        type=Path,
        help="optional cooperative project lock held for the complete capture",
    )
    parser.add_argument(
        "--metadata-output",
        type=Path,
        help="metadata sidecar (default: OUTPUT.metadata.json)",
    )
    parser.add_argument(
        "--rate-policy",
        choices=("off", "warn", "fail"),
        default="warn",
        help="action when received bytes exceed the 8N1 line-rate envelope",
    )
    parser.add_argument(
        "--rate-queue-allowance",
        type=int,
        default=DEFAULT_QUEUE_ALLOWANCE,
        help="initial queued-byte allowance used by the rate check",
    )
    parser.add_argument(
        "--replay-policy",
        choices=("off", "warn", "fail"),
        default="fail",
        help=(
            "action on a bounded-rate or consecutive-identical-line replay "
            "signature (default: fail and stop capture)"
        ),
    )
    parser.add_argument(
        "--replay-burst-allowance",
        type=int,
        default=DEFAULT_REPLAY_BURST_ALLOWANCE,
        help=(
            "maximum queued/live burst accepted before the 8N1 token bucket "
            "must refill"
        ),
    )
    parser.add_argument(
        "--replay-identical-lines",
        type=int,
        default=DEFAULT_REPLAY_IDENTICAL_LINES,
        help="consecutive identical complete lines which trigger replay detection",
    )
    parser.add_argument(
        "--replay-min-line-bytes",
        type=int,
        default=DEFAULT_REPLAY_MIN_LINE_BYTES,
        help="minimum complete-line length eligible for replay detection",
    )
    parser.add_argument(
        "--collapse-identical-lines",
        action="store_true",
        help="also write a derived RLE JSONL view; raw capture remains unchanged",
    )
    parser.add_argument(
        "--collapsed-output",
        type=Path,
        help="derived RLE output (default: OUTPUT.rle.jsonl)",
    )
    parser.add_argument(
        "--marker",
        action="append",
        default=[],
        help="ASCII terminal marker; repeat for alternatives",
    )
    args = parser.parse_args(argv)
    if args.quiet < 0:
        parser.error("--quiet must not be negative")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.marker_linger < 0:
        parser.error("--marker-linger must not be negative")
    if args.max_bytes <= 0:
        parser.error("--max-bytes must be positive")
    if args.rate_queue_allowance < 0:
        parser.error("--rate-queue-allowance must not be negative")
    if args.replay_burst_allowance <= 0:
        parser.error("--replay-burst-allowance must be positive")
    if args.replay_identical_lines < 2:
        parser.error("--replay-identical-lines must be at least 2")
    if args.replay_min_line_bytes <= 0:
        parser.error("--replay-min-line-bytes must be positive")
    if args.collapsed_output is not None and not args.collapse_identical_lines:
        parser.error("--collapsed-output requires --collapse-identical-lines")
    return args


def main() -> int:
    args = parse_args()
    markers = tuple(
        marker.encode("ascii")
        for marker in (args.marker or DEFAULT_MARKERS)
    )
    metadata_output = args.metadata_output or Path(f"{args.output}.metadata.json")
    collapsed_output = args.collapsed_output or Path(f"{args.output}.rle.jsonl")
    session_started_monotonic_ns = time.monotonic_ns()
    session_started_utc = datetime.now(timezone.utc).isoformat()
    deadline = time.monotonic() + args.timeout
    fd: int | None = None
    lock_fd: int | None = None
    lock_acquired = False
    tty_exclusive = False
    result: CaptureResult | None = None
    rate: RateAssessment | None = None
    collapsed: dict[str, object] | None = None
    discarded: int | None = None
    error: str | None = None
    exit_code = 1
    try:
        lock_fd = acquire_project_lock(args.lock_file)
        lock_acquired = lock_fd is not None
        fd = os.open(args.device, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
        claim_tty_exclusive(fd)
        tty_exclusive = True
        configure(fd, args.baud)
        if args.start_immediately:
            print(
                f"capture armed immediately on {args.device} at {args.baud}; "
                "queued input preserved",
                flush=True,
            )
        else:
            # A quiet-gated capture deliberately discards pre-arm input.  Also
            # discard stale transmit bytes left by an interrupted command
            # writer; TCIFLUSH would leave those bytes able to reach ROMMON.
            termios.tcflush(fd, termios.TCIOFLUSH)
            print(
                f"waiting for {args.quiet:.3f}s quiet on "
                f"{args.device} at {args.baud}",
                flush=True,
            )
            discarded = wait_for_quiet(fd, args.quiet, deadline)
            termios.tcflush(fd, termios.TCIOFLUSH)
            print(
                f"quiet observed; discarded={discarded}; capture armed",
                flush=True,
            )
        result = capture(
            fd,
            args.output,
            markers,
            deadline,
            args.marker_linger,
            args.max_bytes,
            args.baud,
            args.replay_policy,
            args.replay_burst_allowance,
            args.replay_identical_lines,
            args.replay_min_line_bytes,
        )
        rate = assess_rate(
            result.total,
            result.started_monotonic_ns,
            result.ended_monotonic_ns,
            args.baud,
            args.rate_queue_allowance,
        )
        if args.collapse_identical_lines:
            collapsed = collapse_identical_lines(
                args.output, collapsed_output, result.sha256
            )
        if result.replay is not None and args.replay_policy == "fail":
            error = result.error or "capture stopped by replay guard"
            exit_code = 3
        elif result.error is not None:
            error = result.error
        elif not rate.plausible and args.rate_policy == "fail":
            error = "capture exceeded the plausible 8N1 line-rate envelope"
            exit_code = 2
        else:
            exit_code = 0
    except (OSError, RuntimeError, TimeoutError, ValueError) as exc:
        error = str(exc)
    finally:
        if fd is not None:
            try:
                if tty_exclusive:
                    release_tty_exclusive(fd)
            except OSError as exc:
                if error is None:
                    error = f"cannot release TTY exclusivity: {exc}"
                    exit_code = 1
            finally:
                os.close(fd)
        try:
            release_project_lock(lock_fd)
        except OSError as exc:
            if error is None:
                error = f"cannot release project lock: {exc}"
                exit_code = 1

    session_ended_monotonic_ns = time.monotonic_ns()
    session_ended_utc = datetime.now(timezone.utc).isoformat()
    if rate is not None and not rate.plausible and args.rate_policy != "off":
        print(
            "warning: capture rate is physically implausible for "
            f"{args.baud} 8N1: {rate.observed_bytes_per_second:.1f} B/s "
            f"> {rate.nominal_max_bytes_per_second:.1f} B/s nominal "
            f"(allowance={args.rate_queue_allowance} bytes)",
            file=sys.stderr,
        )
    if (
        result is not None
        and result.replay is not None
        and args.replay_policy != "off"
    ):
        print(
            "warning: replay guard detected "
            f"{result.replay.kind} at raw byte {result.replay.total_bytes}; "
            f"policy={args.replay_policy}",
            file=sys.stderr,
        )

    marker_text = (
        result.matched.decode("ascii", "replace")
        if result is not None and result.matched
        else "none"
    )
    status = "ok"
    if error is not None:
        if exit_code == 2:
            status = "rate-failed"
        elif exit_code == 3:
            status = "replay-failed"
        else:
            status = "error"
    metadata = {
        "schema": "x58-uart-capture/v1",
        "status": status,
        "device": args.device,
        "baud": args.baud,
        "serial_format": "8N1",
        "raw_output": str(args.output),
        "raw_bytes": result.total if result is not None else None,
        "raw_sha256": result.sha256 if result is not None else None,
        "marker": marker_text,
        "error": error,
        "capture_policy": {
            "timeout_seconds": args.timeout,
            "marker_linger_seconds": args.marker_linger,
            "max_bytes": args.max_bytes,
            "markers": [marker.decode("ascii") for marker in markers],
        },
        "quiet": {
            "start_immediately": args.start_immediately,
            "requested_seconds": args.quiet,
            "discarded_bytes": discarded,
        },
        "exclusivity": {
            "tioc_excl": tty_exclusive,
            "lock_file": str(args.lock_file) if args.lock_file else None,
            "lock_acquired": lock_acquired,
        },
        "timing": {
            "session_started_monotonic_ns": session_started_monotonic_ns,
            "session_ended_monotonic_ns": session_ended_monotonic_ns,
            "capture_started_monotonic_ns": (
                result.started_monotonic_ns if result is not None else None
            ),
            "capture_ended_monotonic_ns": (
                result.ended_monotonic_ns if result is not None else None
            ),
            "session_started_utc": session_started_utc,
            "session_ended_utc": session_ended_utc,
        },
        "rate_check": (
            {
                "policy": args.rate_policy,
                "queue_allowance_bytes": args.rate_queue_allowance,
                "elapsed_seconds": rate.elapsed_seconds,
                "observed_bytes_per_second": rate.observed_bytes_per_second,
                "nominal_max_bytes_per_second": (
                    rate.nominal_max_bytes_per_second
                ),
                "allowed_bytes": rate.allowed_bytes,
                "plausible": rate.plausible,
            }
            if rate is not None
            else None
        ),
        "replay_guard": {
            "policy": args.replay_policy,
            "burst_allowance_bytes": args.replay_burst_allowance,
            "identical_line_limit": args.replay_identical_lines,
            "minimum_line_bytes": args.replay_min_line_bytes,
            "max_identical_lines": (
                result.max_identical_lines if result is not None else None
            ),
            "evidence": (
                {
                    "kind": result.replay.kind,
                    "total_bytes": result.replay.total_bytes,
                    "monotonic_ns": result.replay.monotonic_ns,
                    "burst_observed_bytes": (
                        result.replay.burst_observed_bytes
                    ),
                    "burst_allowed_bytes": (
                        result.replay.burst_allowed_bytes
                    ),
                    "identical_lines": result.replay.identical_lines,
                    "identical_bytes": result.replay.identical_bytes,
                    "line_sha256": result.replay.line_sha256,
                    "line_preview_base64": (
                        result.replay.line_preview_base64
                    ),
                }
                if result is not None and result.replay is not None
                else None
            ),
        },
        "collapsed_identical_lines": collapsed,
    }
    try:
        write_json_exclusive(metadata_output, metadata)
    except OSError as metadata_error:
        print(f"error: cannot write metadata: {metadata_error}", file=sys.stderr)
        exit_code = 1

    if error is not None:
        print(f"error: {error}", file=sys.stderr)
    elif result is not None:
        print(
            f"captured={result.total} sha256={result.sha256} "
            f"marker={marker_text!r} output={args.output} "
            f"metadata={metadata_output}",
            flush=True,
        )
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run MicaOS STM32G070 A-K MCU test images.

This script is intended to be run from the repository root on the user's PC.
It flashes each pre-built image with pyOCD, reads the DAPLink virtual serial
port, and stops on the first FAIL/ASSERT mismatch.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial


TESTS = [
    ("default", "g070-default", "[G070-PRIM] PASS", False),
    ("trace", "g070-trace", "[G070-PRIM] PASS", False),
    ("no-timer", "g070-no-timer", "[G070-PRIM] PASS", False),
    ("no-diagnostic", "g070-no-diagnostic", "[G070-PRIM] PASS", False),
    ("assert-probe", "g070-assert-probe", "[G070-FIRST] ASSERT:", True),
]


def run(cmd: list[str]) -> None:
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def flash_image(uid: str, freq: int, elf: Path) -> None:
    run([
        "pyocd",
        "load",
        "-t",
        "stm32g070rbtx",
        "-u",
        uid,
        "-f",
        str(freq),
        str(elf),
    ])


def wait_log(port: str, baud: int, expect: str, expect_assert: bool, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    captured: list[str] = []

    with serial.Serial(port, baudrate=baud, timeout=0.2) as ser:
        ser.reset_input_buffer()
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode(errors="replace").rstrip()
            print(line, flush=True)
            captured.append(line)

            if "FAIL" in line:
                raise RuntimeError("firmware reported FAIL")

            if "ASSERT" in line and not expect_assert:
                raise RuntimeError("firmware reported unexpected ASSERT")

            if expect in line:
                return "\n".join(captured)

    raise TimeoutError(f"timeout waiting for {expect!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM26")
    parser.add_argument("-u", "--uid", default="0123456789AB")
    parser.add_argument("-f", "--frequency", type=int, default=100000)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--root", default=".")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    project = root / "tests" / "mcu" / "stm32g070_cmake" / "Test_STM32G070"

    for name, build_name, expect, expect_assert in TESTS:
        elf = project / "build" / build_name / "Test_STM32G070.elf"
        if not elf.exists():
            print(f"missing image: {elf}", file=sys.stderr)
            return 2

        print(f"\n=== {name} ===", flush=True)
        flash_image(args.uid, args.frequency, elf)
        wait_log(args.port, args.baud, expect, expect_assert, args.timeout)
        print(f"=== {name}: PASS ===", flush=True)

    print("\nG070 A-K MCU tests PASS", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Build, flash, and run a 40-minute STM32G070 scheduler stress test."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial


def run(cmd: list[str]) -> None:
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def configure_and_build(root: Path, build_name: str) -> Path:
    project = root / "tests" / "mcu" / "stm32g070_cmake" / "Test_STM32G070"
    build = project / "build" / build_name
    toolchain = project / "cmake" / "gcc-arm-none-eabi.cmake"

    run([
        "cmake",
        "-S",
        str(project),
        "-B",
        str(build),
        "-G",
        "MinGW Makefiles",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DMICAOS_EXTRA_DEFINES=MCU_TEST_SCHED_STRESS",
    ])
    run(["cmake", "--build", str(build), "--parallel"])

    elf = build / "Test_STM32G070.elf"
    if not elf.exists():
        raise FileNotFoundError(elf)
    return elf


def flash(uid: str, frequency: int, elf: Path) -> None:
    run([
        "pyocd",
        "load",
        "-t",
        "stm32g070rbtx",
        "-u",
        uid,
        "-f",
        str(frequency),
        str(elf),
    ])


def capture_log(port: str, baud: int, duration_s: int, log_path: Path) -> int:
    fail = 0
    asserts = 0
    ok = 0
    first_ok: str | None = None
    last_ok: str | None = None
    start = time.monotonic()
    end = start + duration_s

    log_path.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(port, baudrate=baud, timeout=0.2) as ser, \
            log_path.open("w", encoding="utf-8", newline="") as f:
        ser.reset_input_buffer()
        while time.monotonic() < end:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode(errors="replace").rstrip()
            elapsed = time.monotonic() - start
            stamped = f"{elapsed:8.3f}s {line}"
            print(stamped, flush=True)
            f.write(stamped + "\n")
            f.flush()

            if "FAIL" in line:
                fail += 1
                break
            if "ASSERT" in line:
                asserts += 1
                break
            if "[G070-SCHED] OK" in line:
                ok += 1
                if first_ok is None:
                    first_ok = line
                last_ok = line

    print(f"saved: {log_path}")
    print(f"FAIL count: {fail}")
    print(f"ASSERT count: {asserts}")
    print(f"OK count: {ok}")
    if first_ok is not None:
        print(f"first OK: {first_ok}")
    if last_ok is not None:
        print(f"last OK: {last_ok}")

    if fail or asserts:
        return 1

    # Firmware reports every 60 seconds. For a 40-minute run, expect 40 lines;
    # keep a little margin for startup/flashing/serial timing.
    min_ok = max(1, (duration_s // 60) - 1)
    if ok < min_ok:
        print(f"too few OK lines: expected at least {min_ok}", file=sys.stderr)
        return 1

    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--port", default="COM26")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("-u", "--uid", default="0123456789AB")
    parser.add_argument("-f", "--frequency", type=int, default=100000)
    parser.add_argument("--minutes", type=int, default=40)
    parser.add_argument("--build-name", default="g070-sched-40min")
    parser.add_argument(
        "--log",
        default="tests/mcu/g070_sched_stress_40min.log",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    log_path = root / args.log
    duration_s = args.minutes * 60

    elf = configure_and_build(root, args.build_name)
    flash(args.uid, args.frequency, elf)
    return capture_log(args.port, args.baud, duration_s, log_path)


if __name__ == "__main__":
    raise SystemExit(main())

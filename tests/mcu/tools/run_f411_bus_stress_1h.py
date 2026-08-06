#!/usr/bin/env python3
"""Build, flash, and run a 1-hour STM32F411 bus stress test."""

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
    project = root / "tests" / "mcu" / "NUCLEO_F411RE_cmake" / "Test_STM32F411"
    build = project / "build" / build_name
    toolchain = project / "cmake" / "gcc-arm-none-eabi.cmake"

    run([
        "cmake",
        "-S",
        str(project),
        "-B",
        str(build),
        "-G",
        "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DMICAOS_EXTRA_DEFINES=MCU_TEST_BUS_STRESS",
    ])
    run(["cmake", "--build", str(build)])

    elf = build / "Test_STM32F411.elf"
    if not elf.exists():
        raise FileNotFoundError(elf)
    return elf


def flash(sn: str, elf: Path) -> None:
    run([
        "STM32_Programmer_CLI",
        "-c",
        "port=SWD",
        f"sn={sn}",
        "mode=UR",
        "-w",
        str(elf),
        "-v",
        "-rst",
    ])


def reset(sn: str) -> None:
    subprocess.run([
        "STM32_Programmer_CLI",
        "-c",
        "port=SWD",
        f"sn={sn}",
        "mode=UR",
        "-rst",
    ], check=True, capture_output=True, text=True)


def capture_log(port: str, baud: int, sn: str, duration_s: int, log_path: Path) -> int:
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
        reset(sn)

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
            if "[F411-BUS] OK" in line:
                ok += 1
                if first_ok is None:
                    first_ok = line
                last_ok = line
                if ("publish_fail=0" not in line) or ("err=0" not in line):
                    fail += 1
                    break

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

    min_ok = max(1, (duration_s // 60) - 1)
    if ok < min_ok:
        print(f"too few OK lines: expected at least {min_ok}", file=sys.stderr)
        return 1

    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--port", default="COM27")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--sn", default="0670FF323535474B43021337")
    parser.add_argument("--minutes", type=int, default=60)
    parser.add_argument("--build-name", default="f411-bus-stress-1h")
    parser.add_argument("--log", default="tests/mcu/f411_bus_stress_1h.log")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    duration_s = args.minutes * 60
    log_path = root / args.log

    elf = configure_and_build(root, args.build_name)
    flash(args.sn, elf)
    return capture_log(args.port, args.baud, args.sn, duration_s, log_path)


if __name__ == "__main__":
    raise SystemExit(main())

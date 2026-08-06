#!/usr/bin/env python3
"""
Race-connect and mass erase an STM32G0 whose firmware disables SWD early.

Use case:
  - The CMSIS-DAP/DAPLink probe is visible to the PC.
  - The target MCU is running old firmware that quickly reconfigures SWD pins.
  - The board has no NRST connection, so normal connect-under-reset is not
    available.

This script intentionally uses pyOCD's generic cortex_m target and directly
writes STM32G0 FLASH registers. This avoids the STM32 device-pack flash
initialization path, which may be too slow or may abort after the narrow
power-on attach window.

Procedure:
  1. Power off the board.
  2. Start this script.
  3. Power on the board immediately after attempts begin.
  4. If the race succeeds, the script halts the core and mass-erases Flash.

This is an emergency recovery helper. Prefer a debugger with NRST connected
for normal development.
"""

from __future__ import annotations

import argparse
import sys
import time

from pyocd.core.helpers import ConnectHelper


CPUID = 0xE000ED00
STM32G0_CPUID = 0x410CC601

FLASH_BASE = 0x40022000
FLASH_KEYR = FLASH_BASE + 0x08
FLASH_SR = FLASH_BASE + 0x10
FLASH_CR = FLASH_BASE + 0x14

FLASH_KEY1 = 0x45670123
FLASH_KEY2 = 0xCDEF89AB

FLASH_SR_CLEAR_ERRORS = 0x0000C3FA
FLASH_SR_BSY1 = 1 << 16

FLASH_CR_MER1 = 1 << 2
FLASH_CR_STRT = 1 << 16
FLASH_CR_LOCK = 1 << 31

FLASH_START = 0x08000000


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Race-connect to STM32G0 via CMSIS-DAP and mass erase Flash."
    )
    parser.add_argument(
        "-u",
        "--uid",
        default="0123456789AB",
        help="CMSIS-DAP probe unique ID. Default: 0123456789AB",
    )
    parser.add_argument(
        "-f",
        "--frequency",
        type=int,
        default=100_000,
        help="SWD frequency in Hz. Default: 100000",
    )
    parser.add_argument(
        "--attempts",
        type=int,
        default=500,
        help="Maximum race attempts. Default: 500",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.01,
        help="Delay between attempts in seconds. Default: 0.01",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="Skip reading Flash after erase.",
    )
    return parser.parse_args()


def wait_flash_ready(target, timeout_s: float = 5.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        sr = target.read32(FLASH_SR)
        if (sr & FLASH_SR_BSY1) == 0:
            return
    raise TimeoutError("FLASH busy did not clear")


def mass_erase_stm32g0(target) -> None:
    target.halt()

    cr = target.read32(FLASH_CR)
    if (cr & FLASH_CR_LOCK) != 0:
        target.write32(FLASH_KEYR, FLASH_KEY1)
        target.write32(FLASH_KEYR, FLASH_KEY2)
        cr = target.read32(FLASH_CR)
        if (cr & FLASH_CR_LOCK) != 0:
            raise RuntimeError("FLASH unlock failed")

    wait_flash_ready(target)

    # Clear stale error/status flags before starting a new erase operation.
    target.write32(FLASH_SR, FLASH_SR_CLEAR_ERRORS)

    target.write32(FLASH_CR, FLASH_CR_MER1)
    target.write32(FLASH_CR, FLASH_CR_MER1 | FLASH_CR_STRT)
    wait_flash_ready(target)

    # Leave mass erase bit cleared. Do not relock: the debugger disconnect will
    # reset target state for normal use.
    target.write32(FLASH_CR, 0)


def try_once(args: argparse.Namespace, attempt: int) -> bool:
    try:
        with ConnectHelper.session_with_chosen_probe(
            unique_id=args.uid,
            target_override="cortex_m",
            frequency=args.frequency,
            connect_mode="attach",
            options={"resume_on_disconnect": False},
        ) as session:
            target = session.board.target
            cpuid = target.read32(CPUID)
            print(f"attempt {attempt}: connected, CPUID=0x{cpuid:08X}")
            if cpuid != STM32G0_CPUID:
                print("warning: CPUID is not STM32G0 Cortex-M0+, continuing")

            mass_erase_stm32g0(target)

            if not args.no_verify:
                value = target.read32(FLASH_START)
                print(f"verify flash[0]=0x{value:08X}")
                if value != 0xFFFFFFFF:
                    raise RuntimeError("Flash verify failed; first word is not erased")

            print("ERASED")
            return True
    except Exception as exc:  # noqa: BLE001 - recovery loop must keep trying.
        print(f"attempt {attempt}: {exc}")
        return False


def main() -> int:
    args = parse_args()

    print("Power off the board, start this script, then power on immediately.")
    print(
        f"probe={args.uid} frequency={args.frequency}Hz "
        f"attempts={args.attempts} delay={args.delay}s"
    )

    for attempt in range(1, args.attempts + 1):
        if try_once(args, attempt):
            return 0
        time.sleep(args.delay)

    print("FAILED")
    return 1


if __name__ == "__main__":
    sys.exit(main())

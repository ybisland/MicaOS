#!/usr/bin/env python3
"""
Offline checker for MicaOS bus explicit configuration tables.

This script checks the relationship between:
  - BUS_CHANNELS_REGISTER(...)
  - BUS_SUBSCRIBERS_REGISTER(...)
  - BUS_SUBSCRIPTIONS_REGISTER(...)

It is intentionally not a C parser. It handles the normal MicaOS bus_config.c
style and reports configuration mistakes before flashing firmware.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


REGISTER_MACROS = (
    "BUS_CHANNELS_REGISTER",
    "BUS_SUBSCRIBERS_REGISTER",
    "BUS_SUBSCRIPTIONS_REGISTER",
)


@dataclass
class MacroCall:
    name: str
    args: str
    file: Path
    line: int


def strip_comments(text: str) -> str:
    """Remove C and C++ comments while preserving line numbers."""
    result: list[str] = []
    i = 0
    in_block = False
    in_line = False
    in_string = False
    in_char = False
    escape = False

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if (i + 1) < len(text) else ""

        if in_block:
            if ch == "\n":
                result.append("\n")
            elif ch == "*" and nxt == "/":
                in_block = False
                i += 1
            else:
                result.append(" ")
        elif in_line:
            if ch == "\n":
                in_line = False
                result.append("\n")
            else:
                result.append(" ")
        elif in_string:
            result.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
        elif in_char:
            result.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "'":
                in_char = False
        elif ch == "/" and nxt == "*":
            in_block = True
            result.append(" ")
            result.append(" ")
            i += 1
        elif ch == "/" and nxt == "/":
            in_line = True
            result.append(" ")
            result.append(" ")
            i += 1
        else:
            result.append(ch)
            if ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True

        i += 1

    return "".join(result)


def line_number_at(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def find_macro_calls(text: str, path: Path, macro_name: str) -> list[MacroCall]:
    calls: list[MacroCall] = []
    pattern = re.compile(r"\b" + re.escape(macro_name) + r"\s*\(")
    pos = 0

    while True:
        match = pattern.search(text, pos)
        if match is None:
            break

        open_index = text.find("(", match.start())
        depth = 0
        i = open_index
        in_string = False
        in_char = False
        escape = False

        while i < len(text):
            ch = text[i]

            if in_string:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == '"':
                    in_string = False
            elif in_char:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == "'":
                    in_char = False
            elif ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    args = text[open_index + 1:i]
                    calls.append(
                        MacroCall(
                            name=macro_name,
                            args=args,
                            file=path,
                            line=line_number_at(text, match.start()),
                        )
                    )
                    i += 1
                    break

            i += 1

        pos = i

    return calls


def parse_single_arg_items(args: str, item_macro: str) -> list[str]:
    pattern = re.compile(
        r"\b" + re.escape(item_macro) + r"\s*\(\s*([A-Za-z_]\w*)\s*\)"
    )
    return [match.group(1) for match in pattern.finditer(args)]


def parse_subscribe_items(args: str) -> list[tuple[str, str]]:
    pattern = re.compile(
        r"\bBUS_SUBSCRIBE\s*\(\s*([A-Za-z_]\w*)\s*,\s*([A-Za-z_]\w*)\s*\)"
    )
    return [(match.group(1), match.group(2)) for match in pattern.finditer(args)]


def parse_definitions(text: str, macro_name: str) -> set[str]:
    pattern = re.compile(
        r"\b" + re.escape(macro_name) + r"\s*\(\s*([A-Za-z_]\w*)\s*,?"
    )
    return {match.group(1) for match in pattern.finditer(text)}


def find_duplicates(items: list[str]) -> set[str]:
    seen: set[str] = set()
    duplicates: set[str] = set()

    for item in items:
        if item in seen:
            duplicates.add(item)
        else:
            seen.add(item)

    return duplicates


def find_duplicate_pairs(items: list[tuple[str, str]]) -> set[tuple[str, str]]:
    seen: set[tuple[str, str]] = set()
    duplicates: set[tuple[str, str]] = set()

    for item in items:
        if item in seen:
            duplicates.add(item)
        else:
            seen.add(item)

    return duplicates


def format_location(call: MacroCall | None) -> str:
    if call is None:
        return "<missing>"
    return f"{call.file}:{call.line}"


def expect_one_call(calls: list[MacroCall], macro_name: str) -> tuple[MacroCall | None, list[str]]:
    errors: list[str] = []

    if len(calls) == 0:
        errors.append(f"missing {macro_name}(...)")
        return None, errors

    if len(calls) > 1:
        locations = ", ".join(format_location(call) for call in calls)
        errors.append(f"{macro_name}(...) must appear exactly once: {locations}")

    return calls[0], errors


def check(paths: list[Path]) -> int:
    texts: list[tuple[Path, str]] = []
    errors: list[str] = []
    warnings: list[str] = []

    for path in paths:
        if not path.exists():
            errors.append(f"file not found: {path}")
            continue
        try:
            raw = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            raw = path.read_text(encoding="gbk")
        texts.append((path, strip_comments(raw)))

    all_text = "\n".join(text for _, text in texts)
    calls_by_macro: dict[str, list[MacroCall]] = {name: [] for name in REGISTER_MACROS}

    for path, text in texts:
        for name in REGISTER_MACROS:
            calls_by_macro[name].extend(find_macro_calls(text, path, name))

    channel_call, call_errors = expect_one_call(
        calls_by_macro["BUS_CHANNELS_REGISTER"], "BUS_CHANNELS_REGISTER"
    )
    errors.extend(call_errors)

    subscriber_call, call_errors = expect_one_call(
        calls_by_macro["BUS_SUBSCRIBERS_REGISTER"], "BUS_SUBSCRIBERS_REGISTER"
    )
    errors.extend(call_errors)

    subscription_call, call_errors = expect_one_call(
        calls_by_macro["BUS_SUBSCRIPTIONS_REGISTER"], "BUS_SUBSCRIPTIONS_REGISTER"
    )
    errors.extend(call_errors)

    channels = (
        parse_single_arg_items(channel_call.args, "BUS_CHANNEL")
        if channel_call is not None
        else []
    )
    subscribers = (
        parse_single_arg_items(subscriber_call.args, "BUS_SUBSCRIBER")
        if subscriber_call is not None
        else []
    )
    subscriptions = (
        parse_subscribe_items(subscription_call.args)
        if subscription_call is not None
        else []
    )

    if channel_call is not None and len(channels) == 0:
        errors.append(f"{format_location(channel_call)}: no BUS_CHANNEL(...) entries")
    elif len(channels) > 0xFFFF:
        errors.append(f"{format_location(channel_call)}: too many channels: {len(channels)}")

    if subscriber_call is not None and len(subscribers) == 0:
        errors.append(f"{format_location(subscriber_call)}: no BUS_SUBSCRIBER(...) entries")
    elif len(subscribers) > 0xFFFF:
        errors.append(
            f"{format_location(subscriber_call)}: too many subscribers: {len(subscribers)}"
        )

    if subscription_call is not None and len(subscriptions) == 0:
        errors.append(f"{format_location(subscription_call)}: no BUS_SUBSCRIBE(...) entries")
    elif len(subscriptions) > 0xFFFF:
        errors.append(
            f"{format_location(subscription_call)}: too many subscriptions: "
            f"{len(subscriptions)}"
        )

    for channel in sorted(find_duplicates(channels)):
        errors.append(f"duplicate channel registration: {channel}")

    for subscriber in sorted(find_duplicates(subscribers)):
        errors.append(f"duplicate subscriber registration: {subscriber}")

    channel_set = set(channels)
    subscriber_set = set(subscribers)

    for channel, subscriber in subscriptions:
        if channel not in channel_set:
            errors.append(
                f"subscription references unregistered channel: {channel} -> {subscriber}"
            )
        if subscriber not in subscriber_set:
            errors.append(
                f"subscription references unregistered subscriber: {channel} -> {subscriber}"
            )

    for channel, subscriber in sorted(find_duplicate_pairs(subscriptions)):
        errors.append(f"duplicate subscription: {channel} -> {subscriber}")

    subscribed_channels = {channel for channel, _ in subscriptions}
    for channel in sorted(channel_set - subscribed_channels):
        warnings.append(f"registered channel has no subscriber: {channel}")

    defined_channels = parse_definitions(all_text, "BUS_EVENT_CHANNEL_DEFINE")
    defined_channels.update(parse_definitions(all_text, "BUS_STATE_CHANNEL_DEFINE"))
    defined_subscribers = parse_definitions(all_text, "BUS_SUBSCRIBER_DEFINE")

    if len(defined_channels) > 0:
        for channel in sorted(channel_set - defined_channels):
            warnings.append(f"registered channel definition not found in checked files: {channel}")

    if len(defined_subscribers) > 0:
        for subscriber in sorted(subscriber_set - defined_subscribers):
            warnings.append(
                f"registered subscriber definition not found in checked files: {subscriber}"
            )

    for warning in warnings:
        print(f"warning: {warning}")

    if len(errors) > 0:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    print(
        "bus config OK: "
        f"{len(channels)} channel(s), "
        f"{len(subscribers)} subscriber(s), "
        f"{len(subscriptions)} subscription(s)"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check MicaOS bus explicit configuration tables."
    )
    parser.add_argument(
        "files",
        nargs="+",
        type=Path,
        help="C source/header files containing bus configuration.",
    )
    args = parser.parse_args()

    return check(args.files)


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Compile ESPHome config and enforce coarse memory/image budgets."""

from __future__ import annotations

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
from typing import Dict

MAX_FLASH_TEXT_BYTES = 620000
MAX_FLASH_RODATA_BYTES = 280000
MAX_DRAM_BSS_BYTES = 26000
MAX_DRAM_DATA_BYTES = 24000
MAX_FIRMWARE_BIN_BYTES = 960000


def parse_build_path(output: str) -> str:
    match = re.search(r"Build path:\s*(\S+)", output)
    if not match:
        raise ValueError("Could not find ESPHome build path in compile output")
    return match.group(1)


def find_size_tool() -> str:
    candidates = [
        shutil.which("xtensa-esp32s3-elf-size"),
        os.path.expanduser(
            "~/.platformio/tools/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-size"
        ),
        os.path.expanduser(
            "~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-size"
        ),
    ]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise FileNotFoundError("xtensa-esp32s3-elf-size not found")


def parse_size_sections(size_output: str) -> Dict[str, int]:
    sections: Dict[str, int] = {}
    for line in size_output.splitlines():
        match = re.match(r"^(\.[^\s]+)\s+([0-9]+)\s+", line.strip())
        if match:
            sections[match.group(1)] = int(match.group(2))
    required = [".flash.text", ".flash.rodata", ".dram0.bss", ".dram0.data"]
    missing = [name for name in required if name not in sections]
    if missing:
        raise ValueError(
            "Missing sections from size output: " + ", ".join(missing)
        )
    return sections


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "config",
        nargs="?",
        default="thermostat-debug.yaml",
        help="ESPHome config to compile",
    )
    args = parser.parse_args()

    compile_cmd = [".venv/bin/esphome", "compile", args.config]
    compile_proc = subprocess.run(compile_cmd, capture_output=True, text=True)
    compile_output = compile_proc.stdout + compile_proc.stderr
    sys.stdout.write(compile_output)
    if compile_proc.returncode != 0:
        return compile_proc.returncode

    build_path = parse_build_path(compile_output)
    env_glob = os.path.join(build_path, ".pioenvs", "*")
    env_dirs = sorted(glob.glob(env_glob))
    if not env_dirs:
        raise FileNotFoundError(f"No .pioenvs directories found under {build_path}")
    env_dir = env_dirs[0]
    firmware_elf = os.path.join(env_dir, "firmware.elf")
    firmware_bin = os.path.join(env_dir, "firmware.bin")
    if not os.path.isfile(firmware_elf):
        raise FileNotFoundError(f"Missing firmware ELF: {firmware_elf}")
    if not os.path.isfile(firmware_bin):
        raise FileNotFoundError(f"Missing firmware bin: {firmware_bin}")

    size_tool = find_size_tool()
    size_proc = subprocess.run(
        [size_tool, "-A", firmware_elf], capture_output=True, text=True, check=True
    )
    sections = parse_size_sections(size_proc.stdout)
    firmware_bin_size = os.path.getsize(firmware_bin)

    failures = []
    if sections[".flash.text"] > MAX_FLASH_TEXT_BYTES:
        failures.append(
            f".flash.text {sections['.flash.text']} > {MAX_FLASH_TEXT_BYTES}"
        )
    if sections[".flash.rodata"] > MAX_FLASH_RODATA_BYTES:
        failures.append(
            f".flash.rodata {sections['.flash.rodata']} > {MAX_FLASH_RODATA_BYTES}"
        )
    if sections[".dram0.bss"] > MAX_DRAM_BSS_BYTES:
        failures.append(f".dram0.bss {sections['.dram0.bss']} > {MAX_DRAM_BSS_BYTES}")
    if sections[".dram0.data"] > MAX_DRAM_DATA_BYTES:
        failures.append(
            f".dram0.data {sections['.dram0.data']} > {MAX_DRAM_DATA_BYTES}"
        )
    if firmware_bin_size > MAX_FIRMWARE_BIN_BYTES:
        failures.append(
            f"firmware.bin {firmware_bin_size} > {MAX_FIRMWARE_BIN_BYTES}"
        )

    print(
        "\nMemory snapshot:"
        f" flash.text={sections['.flash.text']},"
        f" flash.rodata={sections['.flash.rodata']},"
        f" dram0.bss={sections['.dram0.bss']},"
        f" dram0.data={sections['.dram0.data']},"
        f" firmware.bin={firmware_bin_size}"
    )

    if failures:
        print("\nMemory budget check failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Memory budget check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

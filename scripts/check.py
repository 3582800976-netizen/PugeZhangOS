#!/usr/bin/env python3
"""Build and independently exercise Lab 1–3 through the emulated UART.

Only the Python standard library and the existing RISC-V/QEMU tools are used.
The monitor runs inside the kernel; it is not a user process or a Unix shell.
Every invocation replaces its own evidence files with this invocation's results.
"""

from __future__ import annotations

import argparse
import datetime
import os
from pathlib import Path
import re
import selectors
import shlex
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
PROMPT = b"pgos> "
PANIC = re.compile(r"(?:\[PANIC\]|PANIC:|^\[FAIL\]|\[test\][^\n]*\bFAIL\b)", re.MULTILINE | re.IGNORECASE)


class CheckFailure(Exception):
    """An observed result did not satisfy the runtime contract."""


def require(condition: bool, description: str) -> None:
    if not condition:
        raise CheckFailure(description)


def readable(data: bytes) -> str:
    return data.decode("utf-8", errors="replace").replace("\r\n", "\n")


class Machine:
    """One QEMU process, with bounded reads and explicit process cleanup."""

    def __init__(self, argv: list[str], log: Path):
        self.argv = argv
        self.log = log
        self.started = datetime.datetime.now(datetime.timezone.utc).isoformat()
        self.output = bytearray()
        self.process = subprocess.Popen(
            argv,
            cwd=ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)

    def pump(self, duration: float = 0.05) -> None:
        """Read once; selectors bounds waiting even when the guest hangs."""
        for key, _ in self.selector.select(max(0.0, duration)):
            data = os.read(key.fileobj.fileno(), 65536)
            if data:
                self.output.extend(data)
            else:
                self.selector.unregister(key.fileobj)

    def wait_for(self, token: bytes, start: int = 0, timeout: float = 8.0) -> bytes:
        deadline = time.monotonic() + timeout
        while token not in self.output[start:]:
            if self.process.poll() is not None:
                self.pump(0)
                if token in self.output[start:]:
                    break
                raise CheckFailure(
                    f"QEMU exited {self.process.returncode} before {token!r}; "
                    f"last output:\n{readable(bytes(self.output[-1600:]))}"
                )
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise CheckFailure(
                    f"timeout waiting for {token!r}; "
                    f"last output:\n{readable(bytes(self.output[-1600:]))}"
                )
            self.pump(min(0.1, remaining))
        return bytes(self.output[start:])

    def drain_for(self, duration: float) -> None:
        """Let interrupt counters advance while still consuming guest output."""
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            self.pump(min(0.05, deadline - time.monotonic()))

    def send(self, data: bytes) -> None:
        assert self.process.stdin is not None
        try:
            self.process.stdin.write(data)
            self.process.stdin.flush()
        except BrokenPipeError as error:
            raise CheckFailure("QEMU closed its UART input unexpectedly") from error

    def command(self, data: str | bytes, timeout: float = 5.0) -> str:
        self.drain_for(0.025)
        start = len(self.output)
        self.send(data.encode() + b"\n" if isinstance(data, str) else data)
        self.wait_for(PROMPT, start, timeout)
        self.drain_for(0.025)
        reply = readable(bytes(self.output[start:]))
        require(PANIC.search(reply) is None, f"guest reported a failure:\n{reply}")
        return reply

    def shutdown(self) -> None:
        start = len(self.output)
        self.send(b"quit\n")
        self.wait_for(b"[shutdown]", start, timeout=5)
        deadline = time.monotonic() + 5
        while self.process.poll() is None and time.monotonic() < deadline:
            self.pump(0.05)
        require(self.process.poll() is not None, "quit did not stop QEMU")
        self.pump(0)
        require(self.process.returncode == 0, f"QEMU shutdown exit={self.process.returncode}")

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        self.pump(0)
        self.selector.close()
        self.log.write_bytes(
            (f"Agent-run UART observation, started {self.started}\n"
             "$ " + shlex.join(self.argv) + "\n\n").encode() + self.output
        )
        if self.process.stdin is not None:
            self.process.stdin.close()
        if self.process.stdout is not None:
            self.process.stdout.close()


def build(lab: int, cpus: int, directory: Path) -> Path:
    argv = ["make", "--no-print-directory", f"LAB={lab}", f"CPUS={cpus}"]
    result = subprocess.run(
        argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120
    )
    (directory / f"lab{lab}-cpu{cpus}-build.log").write_bytes(
        ("$ " + shlex.join(argv) + "\n\n").encode() + result.stdout
    )
    require(result.returncode == 0, f"build failed:\n{readable(result.stdout)}")
    return ROOT / "build" / f"lab{lab}-cpu{cpus}" / "kernel.elf"


def check_elf(kernel: Path, lab: int, toolprefix: str, directory: Path) -> None:
    argv = [toolprefix + "readelf", "-h", str(kernel)]
    result = subprocess.run(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=5,
    )
    records = ("$ " + shlex.join(argv) + "\n\n").encode() + result.stdout
    nm_argv = [toolprefix + "nm", "-n", str(kernel)]
    symbols = subprocess.run(nm_argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
    records += ("\n$ " + shlex.join(nm_argv) + "\n\n").encode() + symbols.stdout
    (directory / f"{kernel.parent.name}-elf.log").write_bytes(records)
    require(result.returncode == 0, f"readelf failed: {readable(result.stdout)}")
    header = readable(result.stdout)
    entry = re.search(r"Entry point address:\s*(0x[0-9a-fA-F]+)", header)
    expected = 0x80000000 if lab == 1 else 0x80200000
    require(entry is not None and int(entry[1], 16) == expected,
            f"ELF entry must be {expected:#x}:\n{header}")
    require("RISC-V" in header and "ELF64" in header, "ELF is not RISC-V ELF64")
    require(symbols.returncode == 0, "nm could not inspect kernel symbols")
    boot = re.search(r"^([0-9a-fA-F]+)\s+T\s+_boot$", readable(symbols.stdout), re.MULTILINE)
    require(boot is not None and int(boot[1], 16) == expected,
            f"_boot is not at the ELF entry {expected:#x}")


def check_boot(reply: str, lab: int, cpus: int, leader: int) -> None:
    rows = re.findall(r"^\[boot\] hart=(\d+) ([^\n]+)", reply, re.MULTILINE)
    require(len(rows) == cpus, f"expected {cpus} boot rows, saw {len(rows)}:\n{reply}")
    require({int(hart) for hart, _ in rows} == set(range(cpus)), "boot hart IDs differ")
    stacks: list[tuple[int, int]] = []
    for hart, row in rows:
        require("ready=1" in row, f"hart {hart} did not publish readiness")
        stages = re.search(r"stages=(0x[0-9a-fA-F]+)", row)
        require(stages is not None, f"hart {hart} lacks actual startup stages")
        assert stages is not None
        required_stages = 0x67 if lab == 1 else 0x77
        actual_stages = int(stages[1], 16)
        require(actual_stages & required_stages == required_stages,
                f"hart {hart} missing completed stages: {actual_stages:#x}")
        if lab == 1:
            require(actual_stages & 0x10 == 0, "Lab 1 spuriously reported paging enabled")
        require(bool(actual_stages & 0x08) == (int(hart) == leader),
                f"global initialization not restricted to boot leader {leader}")
        stack = re.search(r"stack=(0x[0-9a-fA-F]+)\.\.(0x[0-9a-fA-F]+)", row)
        require(stack is not None, f"hart {hart} lacks stack bounds: {row}")
        assert stack is not None
        low, high = int(stack[1], 16), int(stack[2], 16)
        require(high - low == 4096, f"hart {hart} stack is not 4 KiB")
        require(low % 4096 == 0, f"hart {hart} stack is not page aligned")
        stacks.append((low, high))
        satp = re.search(r"satp=(0x[0-9a-fA-F]+)", row)
        require(satp is not None, f"hart {hart} lacks satp")
        assert satp is not None
        mode = int(satp[1], 16) >> 60
        require(mode == (0 if lab == 1 else 8), f"hart {hart} satp mode={mode}")
    stacks.sort()
    require(all(left[1] <= right[0] for left, right in zip(stacks, stacks[1:])),
            "hart stack reservations overlap")


def memory_snapshot(reply: str) -> dict[str, tuple[int, int, int, int]]:
    rows = re.findall(
        r"^\[mem\] pool=(kernel|user) total=(\d+) free=(\d+) "
        r"allocated=(\d+) rejected=(\d+)", reply, re.MULTILINE
    )
    require(len(rows) == 2, f"expected two physical page pools:\n{reply}")
    pools = {name: tuple(map(int, numbers)) for name, *numbers in rows}
    for name, (total, free, allocated, _rejected) in pools.items():
        require(total > 0 and free > 0, f"{name} pool has no available physical pages")
        require(free + allocated == total, f"{name} counters violate total=free+allocated")
    return pools


def timer_snapshot(reply: str, cpus: int) -> dict[int, int]:
    rows = re.findall(r"^\[ticks\] hart=(\d+) count=(\d+)", reply, re.MULTILINE)
    ticks = {int(hart): int(count) for hart, count in rows}
    require(len(rows) == cpus and set(ticks) == set(range(cpus)),
            f"missing or duplicate per-hart timer counts:\n{reply}")
    return ticks


def irq_snapshot(reply: str) -> tuple[int, int, int]:
    row = re.search(r"^\[irq\] uart_rx=(\d+) bytes=(\d+) dropped=(\d+)", reply, re.MULTILINE)
    require(row is not None, f"missing UART interrupt counters:\n{reply}")
    assert row is not None
    return tuple(map(int, row.groups()))


def check_echo(machine: Machine, data: bytes, expected: str) -> None:
    reply = machine.command(data)
    require(re.search(r"^\[echo\] " + re.escape(expected) + r"\r?$", reply, re.MULTILINE)
            is not None, f"echo/editing expected {expected!r}:\n{reply}")


def exercise(lab: int, cpus: int, qemu: str, kernel: Path, directory: Path) -> list[str]:
    argv = [
        qemu, "-machine", "virt", "-bios", "none" if lab == 1 else "default",
        "-kernel", str(kernel), "-m", "128M", "-smp", str(cpus),
        "-nographic", "-monitor", "none",
    ]
    machine = Machine(argv, directory / f"lab{lab}-cpu{cpus}-runtime.log")
    passed: list[str] = []
    try:
        startup = readable(machine.wait_for(PROMPT, timeout=20))
        require(f"[ready] lab={lab} cpus={cpus}" in startup, "readiness configuration differs")
        require(PANIC.search(startup) is None, f"guest startup failure:\n{startup}")
        ready = re.search(r"\[ready\] lab=\d+ cpus=\d+ leader=(\d+)", startup)
        require(ready is not None, "boot leader missing from readiness record")
        assert ready is not None
        check_boot(machine.command("boot"), lab, cpus, int(ready[1]))
        passed.append("ELF/entry + every hart ready + independent 4 KiB stacks + per-hart satp")

        if lab == 1:
            uart_rows = re.findall(r"^\[uart-test\] hart=(\d+) seq=(\d+)\r?$",
                                   startup, re.MULTILINE)
            expected = {(hart, seq) for hart in range(cpus) for seq in range(4)}
            actual = {(int(hart), int(seq)) for hart, seq in uart_rows}
            require(actual == expected and len(uart_rows) == len(expected),
                    f"concurrent console lines incomplete/interleaved: {uart_rows}")
            passed.append(f"all {cpus * 4} concurrent UART messages intact")

        help_reply = machine.command("help")
        require(all(name in help_reply for name in ("boot", "mem", "vm", "ticks", "irq", "test", "echo", "quit")),
                "help omits supported diagnostic commands")
        tests = machine.command("test", timeout=15)
        require("[test] format PASS" in tests, f"format boundary tests missing:\n{tests}")
        passed.append("formatter built-in boundary tests")

        if lab >= 2:
            require("[test] page-concurrent PASS" in startup,
                    f"multi-hart allocator stress missing:\n{startup}")
            require("[test] page PASS" in tests, f"allocator tests missing:\n{tests}")
            require("[test] vm PASS" in tests, f"virtual memory tests missing:\n{tests}")
            for proof in ("hardware MMU is Sv39", "hardware alias write visible at PA",
                          "hardware PA write visible at alias", "hardware zero-page load rejected",
                          "hardware firmware load rejected", "hardware text write rejected",
                          "hardware rodata write rejected"):
                require("[PASS] lab2 vm " + proof in tests,
                        f"actual CPU address-translation proof missing: {proof}")
            before = memory_snapshot(machine.command("mem"))
            second_tests = machine.command("test", timeout=15)
            require("[test] page PASS" in second_tests and "[test] vm PASS" in second_tests,
                    f"repeated allocator/VM tests failed:\n{second_tests}")
            after = memory_snapshot(machine.command("mem"))
            require(all(before[name][:3] == after[name][:3] for name in before),
                    f"test leaked pages: before={before}, after={after}")
            maps = machine.command("vm")
            for name, permission in (("UART", "RW"), ("TEXT", "RX"), ("DATA", "RW")):
                require(re.search(r"^\[vm\] " + name + r" [^\n]*perms=" + permission + r"\b", maps, re.MULTILINE)
                        is not None, f"{name} permissions not {permission}:\n{maps}")
            for name in ("FIRMWARE", "CLINT"):
                require(re.search(r"^\[vm\] " + name + r" [^\n]*unmapped", maps, re.MULTILINE)
                        is not None, f"{name} unexpectedly mapped:\n{maps}")
            passed.append("allocator+Sv39+actual CPU alias tests; repeat restores both pools; mapping permissions")

        if lab == 3:
            require("[test] trap PASS" in tests, f"trap register-restore probe missing:\n{tests}")
            first_ticks = timer_snapshot(machine.command("ticks"), cpus)
            machine.drain_for(0.35)
            second_ticks = timer_snapshot(machine.command("ticks"), cpus)
            require(all(second_ticks[hart] > first_ticks[hart] for hart in first_ticks),
                    f"timer did not advance on every hart: {first_ticks} -> {second_ticks}")
            before_irq = irq_snapshot(machine.command("irq"))
            passed.append("SBI timer interrupts advance on every active hart")

        check_echo(machine, b"echo CRLF_OK\r\n", "CRLF_OK")
        check_echo(machine, b"echo FIXx\bED\n", "FIXED")
        check_echo(machine, b"echo DELX\x7f_OK\n", "DEL_OK")
        check_echo(machine, b"echo LF_OK\n", "LF_OK")
        check_echo(machine, b"echo " + b"b" * 122 + b"\n", "b" * 122)
        overflow = machine.command(b"echo " + b"x" * 123 + b"\n")
        require("[input] line too long; discarded" in overflow,
                f"128-character line was not rejected:\n{overflow}")
        require("[echo]" not in overflow, "overlong line executed a truncated command")
        large_overflow = machine.command(b"echo " + b"x" * 300 + b"\n")
        require("[input] line too long; discarded" in large_overflow,
                "large overlong line was not discarded")
        check_echo(machine, b"echo RECOVERED\n", "RECOVERED")
        passed.append("UART CRLF/LF/backspace/DEL; 127-character boundary; overflow discard+recovery")

        if lab == 3:
            after_irq = irq_snapshot(machine.command("irq"))
            require(after_irq[0] > before_irq[0] and after_irq[1] > before_irq[1],
                    f"UART IRQ/received-byte counters did not advance: {before_irq} -> {after_irq}")
            require(after_irq[2] == 0, f"UART input ring lost bytes: {after_irq}")
            passed.append("UART PLIC interrupt+byte counters advance with no ring drops")

        machine.shutdown()
        passed.append("quit shuts down QEMU with exit status 0")
        require(PANIC.search(readable(bytes(machine.output))) is None, "guest failure in transcript")
        return passed
    finally:
        machine.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lab", choices=("1", "2", "3", "all"), default="all")
    parser.add_argument("--cpus", default="1,2,3,8", help="comma-separated CPU counts from 1 to 8")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-riscv64"))
    parser.add_argument("--toolprefix", default=os.environ.get("TOOLPREFIX", "riscv64-linux-gnu-"))
    parser.add_argument("--no-build", action="store_true", help="exercise existing build outputs")
    parser.add_argument("--evidence", type=Path, default=ROOT / "docs" / "evidence" / "lab123")
    args = parser.parse_args()
    try:
        cpus = list(dict.fromkeys(int(count) for count in args.cpus.split(",")))
    except ValueError:
        parser.error("--cpus must contain comma-separated integers")
    if not cpus or any(count < 1 or count > 8 for count in cpus):
        parser.error("CPU counts must be from 1 to 8")
    labs = [1, 2, 3] if args.lab == "all" else [int(args.lab)]
    args.evidence.mkdir(parents=True, exist_ok=True)
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    summary = [f"Agent-run runtime verification. Started at {started}",
               "Monitor is S-mode kernel code, not a user shell.", ""]
    failures = 0
    try:
        for lab in labs:
            for count in cpus:
                name = f"Lab {lab}, CPUs={count}"
                print(f"Checking {name} ...", flush=True)
                try:
                    kernel = ROOT / "build" / f"lab{lab}-cpu{count}" / "kernel.elf"
                    if not args.no_build:
                        kernel = build(lab, count, args.evidence)
                    require(kernel.is_file(), f"kernel missing: {kernel}")
                    check_elf(kernel, lab, args.toolprefix, args.evidence)
                    checks = exercise(lab, count, args.qemu, kernel, args.evidence)
                    summary.append(f"PASS {name}")
                    summary.extend("  - " + check for check in checks)
                    print(f"PASS {name} ({len(checks)} groups)", flush=True)
                except (CheckFailure, OSError, subprocess.TimeoutExpired) as error:
                    failures += 1
                    summary.append(f"FAIL {name}: {error}")
                    print(f"FAIL {name}: {error}", flush=True)
    finally:
        summary.extend(["", f"Results: {len(labs) * len(cpus) - failures} passed, {failures} failed."])
        (args.evidence / "summary.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
    print(f"Evidence: {args.evidence / 'summary.txt'}", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

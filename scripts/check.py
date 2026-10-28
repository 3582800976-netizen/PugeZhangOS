#!/usr/bin/env python3
"""Build and independently exercise Lab 1–5 through the emulated UART.

Only the Python standard library and the existing RISC-V/QEMU tools are used.
The monitor runs inside the kernel; it is not a user process or a Unix shell.
Each lab is built from its own directory, without a shared LAB-selected kernel.
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
PANIC = re.compile(r"(?:\[PANIC\]|PANIC:|^\[FAIL\]|\[(?:test|utest|htest|huser)\][^\n]*\bFAIL\b)", re.MULTILINE | re.IGNORECASE)


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
    argv = ["make", "--no-print-directory", "-C", f"lab{lab}", f"CPUS={cpus}"]
    result = subprocess.run(
        argv, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120
    )
    (directory / "build.log").write_bytes(
        ("$ " + shlex.join(argv) + "\n\n").encode() + result.stdout
    )
    require(result.returncode == 0, f"build failed:\n{readable(result.stdout)}")
    return ROOT / f"lab{lab}" / "build" / f"cpu{cpus}" / "kernel.elf"


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
    (directory / "elf.log").write_bytes(records)
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
    if lab >= 4:
        user = kernel.parent / "user" / "first.elf"
        user_argv = [toolprefix + "readelf", "-h", "-S", "-W", str(user)]
        user_elf = subprocess.run(user_argv, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=5)
        with (directory / "elf.log").open("ab") as log:
            log.write(("\n$ " + shlex.join(user_argv) + "\n\n").encode() + user_elf.stdout)
        user_header = readable(user_elf.stdout)
        require(user_elf.returncode == 0 and "RISC-V" in user_header and "ELF64" in user_header,
                "independently linked user ELF is missing or invalid")
        user_entry = re.search(r"Entry point address:\s*(0x[0-9a-fA-F]+)", user_header)
        require(user_entry is not None and int(user_entry[1], 16) == 0x1000,
                "user ELF entry is not USER_CODE_BASE")
        for section, address, permissions, filename in (("text", 0x1000, "AX", "code.bin"),
                                                        ("data", 0x2000, "WA", "data.bin")):
            match = re.search(r"\[\s*\d+\]\s+\." + section +
                              r"\s+PROGBITS\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+(\w+)",
                              user_header)
            require(match is not None, f"user .{section} section missing")
            assert match is not None
            section_address, size, flags = int(match[1], 16), int(match[2], 16), match[3]
            require(section_address == address and 0 < size <= 4096 and flags == permissions,
                    f"user .{section} layout or permissions invalid")
            require((user.parent / filename).stat().st_size == size,
                    f"embedded {filename} does not match its user ELF section")


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


def process_snapshot(reply: str, mode: int) -> dict[str, int | str]:
    row = re.search(
        r"^\[proc\] pid=(\d+) mode=(\d+) origin=(\w+) reason=([\w-]+) "
        r"status=(-?\d+) report=(\d+) calls=(\d+) user_ticks=(\d+) user_irqs=(\d+)",
        reply, re.MULTILINE,
    )
    require(row is not None, f"missing actual process result:\n{reply}")
    assert row is not None
    result: dict[str, int | str] = dict(zip(
        ("pid", "mode", "origin", "reason", "status", "report", "calls", "user_ticks", "user_irqs"),
        row.groups(),
    ))
    for key in ("pid", "mode", "status", "report", "calls", "user_ticks", "user_irqs"):
        result[key] = int(result[key])
    require(result["pid"] > 0 and result["mode"] == mode and result["origin"] == "U",
            f"process was not a real U-mode run of mode {mode}: {result}")
    if mode in (0, 7):
        require(result["reason"] == "exit" and result["status"] == 0,
                f"user program failed: {result}")
        require(result["calls"] >= 8, f"minimal call tests missing: {result}")
        if mode == 0:
            require(result["report"] == 5050 and result["user_ticks"] >= 3,
                    f"user computation or real U timer interrupts missing: {result}")
            require("hello from U-mode" in reply, f"user WRITE did not print its message:\n{reply}")
    elif mode == 8:
        require(result["reason"] == "exit" and result["status"] == 0 and
                result["report"] == 0x5005 and result["calls"] > 0,
                f"real U-mode heap demonstration failed: {result}")
        require(re.search(r"^heap-ok\r?$", reply, re.MULTILINE) is not None,
                f"cross-page heap WRITE did not print its U-mode message:\n{reply}")
    else:
        addresses = {1: 0x80200000, 2: 0x10000000, 3: 0x1000,
                     4: 0x4000000000 - 12288, 5: 0x4000000000 - 8192,
                     6: 0x4000000000 - 4096, 9: 0x3000, 10: 0x3000, 11: 0x3000}
        causes = {1: 13, 2: 13, 3: 15, 4: 12, 5: 13, 6: 13, 9: 13, 10: 15, 11: 12}
        fault = re.search(r"^\[proc-fault\] scause=(0x[0-9a-fA-F]+) "
                          r"sepc=(0x[0-9a-fA-F]+) stval=(0x[0-9a-fA-F]+)", reply, re.MULTILINE)
        require(result["reason"] == "fault" and fault is not None,
                f"protected user access was not contained:\n{reply}")
        assert fault is not None
        cause, pc, value = (int(item, 16) for item in fault.groups())
        require(cause == causes[mode] and value == addresses[mode],
                f"wrong user fault evidence: mode={mode} cause={cause} value={value:#x}")
        if mode in (9, 10, 11):
            require(result["calls"] >= (1 if mode == 11 else 2),
                    f"heap fault mode did not first allocate/free through real syscalls: {result}")
        require(pc == addresses[mode] if mode in (4, 11) else 0x1000 <= pc < 0x2000,
                f"fault PC was outside the relevant user instructions: {pc:#x}")
    return result


def check_user_processes(machine: Machine) -> None:
    before = memory_snapshot(machine.command("mem"))
    for mode in (0, 1, 2, 3, 4, 5, 6, 7, 0):
        process_snapshot(machine.command(f"user {mode}", timeout=20), mode)
        check_echo(machine, f"echo USER_{mode}_RETURNED\n".encode(), f"USER_{mode}_RETURNED")
        after = memory_snapshot(machine.command("mem"))
        require(all(before[name][:3] == after[name][:3] for name in before),
                f"mode {mode} leaked process resources: before={before}, after={after}")
    tests = machine.command("utest", timeout=30)
    require("[utest] summary failures=0" in tests and
            re.search(r"^\[utest\][^\n]*\bFAIL\b", tests, re.MULTILINE) is None,
            f"user-process tests failed:\n{tests}")
    for budget in range(9):
        require(f"[utest] allocation-budget={budget} PASS" in tests,
                f"allocation-failure rollback case {budget} missing")
    for proof in ("memory-valid-cross-page-copy", "memory-bad-second-page-no-partial-copy",
                  "memory-exact-owned-pages", "memory-resource-balance"):
        require(f"[utest] {proof} PASS" in tests,
                f"user memory production-path proof missing: {proof}")
    rows = re.findall(r"^\[utest\] round=(\d+) mode=(\d+) reason=(\d+) "
                      r"status=(-?\d+) report=(\d+) origin=(\d+) cause=(0x[0-9a-fA-F]+) "
                      r"epc=(0x[0-9a-fA-F]+) value=(0x[0-9a-fA-F]+) user_ticks=(\d+) PASS$",
                      tests.replace("\r", ""), re.MULTILINE)
    require(len(rows) == 16 and {(int(row[0]), int(row[1])) for row in rows} ==
            {(round_, mode) for round_ in range(2) for mode in range(8)},
            f"repeated mode/isolation evidence missing:\n{tests}")
    for row in rows:
        _round, mode, reason, status, report, origin, cause, pc, value, ticks = row
        require(int(origin) > 0, "repeated run lacked U-origin evidence")
        if int(mode) == 0:
            require(int(reason) == 0 and int(status) == 0 and int(report) == 5050 and int(ticks) >= 3,
                    "repeated normal user computation or timer proof failed")
        elif int(mode) == 7:
            require(int(reason) == 0 and int(status) == 0, "repeated bad-call recovery failed")
        else:
            expected_cause = 15 if int(mode) == 3 else 12 if int(mode) == 4 else 13
            require(int(reason) == 1 and int(cause, 16) == expected_cause,
                    "repeated protection fault proof failed")
    after = memory_snapshot(machine.command("mem"))
    require(all(before[name][:3] == after[name][:3] for name in before),
            f"repeated user tests leaked pages: before={before}, after={after}")
    check_echo(machine, b"echo UTEST_RETURNED\n", "UTEST_RETURNED")

    # Inject input after user WRITE proves U execution, before the user timer
    # window ends. The kernel monitor consumes the queued line after EXIT.
    machine.drain_for(0.025)
    start = len(machine.output)
    machine.send(b"user 0\n")
    machine.wait_for(b"hello from U-mode", start, timeout=20)
    machine.send(b"echo IRQ_DURING_USER\n")
    marker = b"[echo] IRQ_DURING_USER"
    machine.wait_for(marker, start, timeout=20)
    end = machine.output.find(marker, start) + len(marker)
    machine.wait_for(PROMPT, end, timeout=5)
    reply = readable(bytes(machine.output[start:]))
    result = process_snapshot(reply, 0)
    require(result["user_irqs"] > 0,
            f"UART input was not serviced during actual U execution:\n{reply}")
    after = memory_snapshot(machine.command("mem"))
    require(all(before[name][:3] == after[name][:3] for name in before),
            "UART-interrupted user run leaked pages")


def check_heap_maps(reply: str, rounds: int = 1) -> None:
    """Inspect snapshots of the actual free list at every U-demo operation."""
    base, pages, page_size, node_count = 0x3000, 1024, 4096, 32
    headers = list(re.finditer(
        r"^\[heap-map\] tag=(\d+) base=(0x[0-9a-fA-F]+) limit=(0x[0-9a-fA-F]+) "
        r"total=(\d+) free=(\d+) used=(\d+) regions=(\d+) largest=(\d+) nodes_free=(\d+)\r?$",
        reply, re.MULTILINE,
    ))
    require(len(headers) == rounds * 7,
            f"expected {rounds * 7} actual heap-map snapshots, saw {len(headers)}:\n{reply}")
    expected = (
        [(0, pages)],                         # Initial heap.
        [(7, pages - 7)],                     # A=2, B=3, C=2 allocated.
        [(2, 3), (7, pages - 7)],             # B released between A and C.
        [(4, 1), (7, pages - 7)],             # First two B pages reused.
        [(0, pages)],                         # All allocations released.
        [(0, 20), (22, pages - 22)],          # Exact middle placement splits a region.
        [(0, pages)],                         # Both free neighbors merge again.
    )
    free_row_count = 0
    for ordinal, header in enumerate(headers):
        fields = header.groups()
        tag = int(fields[0])
        actual_base, limit = int(fields[1], 16), int(fields[2], 16)
        total, free, used, regions, largest, nodes_free = map(int, fields[3:])
        require(tag == ordinal % 7, f"heap-map tags are incomplete or out of order: {tag}")
        require(actual_base == base and limit == base + pages * page_size and total == pages,
                f"heap-map bounds differ from the heap contract: {header.group(0)}")
        end = headers[ordinal + 1].start() if ordinal + 1 < len(headers) else len(reply)
        rows = re.findall(r"^\[heap-free\] tag=(\d+) index=(\d+) "
                          r"begin=(0x[0-9a-fA-F]+) pages=(\d+)\r?$",
                          reply[header.end():end], re.MULTILINE)
        free_row_count += len(rows)
        require(len(rows) == regions and all(int(row[0]) == tag for row in rows) and
                [int(row[1]) for row in rows] == list(range(regions)),
                f"heap-map free-list rows are missing, duplicated, or out of order: {rows}")
        spans = [(int(row[2], 16), int(row[3])) for row in rows]
        for begin, count in spans:
            require(count > 0 and begin % page_size == 0 and
                    base <= begin < begin + count * page_size <= limit,
                    f"heap-map has an invalid free interval: {(begin, count)}")
        require(all(left + count * page_size < right
                    for (left, count), (right, _) in zip(spans, spans[1:])),
                f"heap-map has overlapping or unmerged adjacent free intervals: {spans}")
        require(free == sum(count for _, count in spans) and free + used == total and
                largest == max((count for _, count in spans), default=0) and
                nodes_free == node_count - regions,
                f"heap-map resources do not match the actual free list: {header.group(0)} {spans}")
        wanted = [(base + offset * page_size, count) for offset, count in expected[tag]]
        require(spans == wanted,
                f"heap-map tag {tag} does not show the expected split/merge/reuse: {spans} != {wanted}")
    require(len(re.findall(r"^\[heap-free\]", reply, re.MULTILINE)) == free_row_count,
            "heap-free rows appeared outside an actual heap-map snapshot")


def check_user_heap(machine: Machine) -> None:
    before = memory_snapshot(machine.command("mem"))
    demo = machine.command("heap", timeout=20)
    process_snapshot(demo, 8)
    check_heap_maps(demo)
    after = memory_snapshot(machine.command("mem"))
    require(all(before[name][:3] == after[name][:3] for name in before),
            f"heap demonstration leaked process resources: before={before}, after={after}")
    check_echo(machine, b"echo HEAP_RETURNED\n", "HEAP_RETURNED")
    for mode in (9, 10, 11):
        process_snapshot(machine.command(f"user {mode}", timeout=20), mode)
        after = memory_snapshot(machine.command("mem"))
        require(all(before[name][:3] == after[name][:3] for name in before),
                f"heap fault mode {mode} leaked resources: before={before}, after={after}")
        check_echo(machine, f"echo HEAP_{mode}_RETURNED\n".encode(), f"HEAP_{mode}_RETURNED")

    tests = machine.command("htest", timeout=30)
    require("[htest] summary failures=0" in tests and
            "[huser] summary failures=0" in tests and "[heap-check] failures=0" in tests,
            f"heap production-path or repeated-U tests failed:\n{tests}")
    for proof in ("production-create", "initial-region", "front-allocation", "middle-split", "end-allocation",
                  "whole-region", "isolated-insert", "right-merge", "double-merge", "left-merge",
                  "first-fit-skips-small", "zero-rwu-nx", "rollback-existing-data",
                  "invalid-arguments-atomic", "occupied-hint-atomic", "duplicate-free-atomic",
                  "partial-overlap-free-atomic", "failed-map-atomic", "collision-cleanup",
                  "bad-later-leaf-free-atomic", "bad-leaf-cleanup", "node-exhaustion-setup",
                  "node-exhaustion-create",
                  "node-exhaustion-split-atomic", "node-exhaustion-insert-atomic",
                  "node-exhaustion-edge-allocation", "node-exhaustion-adjacent-free",
                  "node-exhaustion-whole-allocation", "node-exhaustion-recovery",
                  "full-allocation-empty-head", "full-allocation-no-space", "full-allocation-release", "reuse-is-zero",
                  "cross-2mib-reclaim-loops", "destroy-live-heap", "metadata-repeat-destroy",
                  "borrowed-content-preserved", "resource-balance"):
        require(f"[htest] {proof} PASS" in tests,
                f"heap production-path proof missing: {proof}")
    for budget in range(5):
        require(f"[htest] allocation-budget={budget} PASS" in tests,
                f"heap allocation rollback budget {budget} missing")
    rows = re.findall(
        r"^\[huser\] round=(\d+) mode=(\d+) reason=(\d+) status=(-?\d+) report=(\d+) "
        r"origin=(\d+) cause=(0x[0-9a-fA-F]+) epc=(0x[0-9a-fA-F]+) "
        r"value=(0x[0-9a-fA-F]+) PASS\r?$", tests, re.MULTILINE,
    )
    require(len(rows) == 8 and {(int(row[0]), int(row[1])) for row in rows} ==
            {(round_, mode) for round_ in range(2) for mode in range(8, 12)},
            f"repeated heap U-mode evidence missing:\n{tests}")
    for row in rows:
        _round, mode, reason, status, report, origin, cause, pc, value = row
        mode_number = int(mode)
        require(int(origin) > 0, "repeated heap run lacked U-origin evidence")
        if mode_number == 8:
            require(int(reason) == 0 and int(status) == 0 and int(report) == 0x5005,
                    "repeated U-mode heap write/read/split/merge/reuse failed")
        else:
            expected_cause = {9: 13, 10: 15, 11: 12}[mode_number]
            require(int(reason) == 1 and int(cause, 16) == expected_cause and
                    int(value, 16) == 0x3000,
                    f"repeated heap fault evidence is wrong: {row}")
            require(int(pc, 16) == 0x3000 if mode_number == 11 else
                    0x1000 <= int(pc, 16) < 0x2000,
                    f"repeated heap fault PC is wrong: {row}")
    check_heap_maps(tests, rounds=2)
    require(len(re.findall(r"^heap-ok\r?$", tests, re.MULTILINE)) == 2,
            "repeated heap U-mode cross-page WRITE evidence missing")
    after = memory_snapshot(machine.command("mem"))
    require(all(before[name][:3] == after[name][:3] for name in before),
            f"heap tests leaked pages: before={before}, after={after}")
    check_echo(machine, b"echo HTEST_RETURNED\n", "HTEST_RETURNED")


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
    machine = Machine(argv, directory / "runtime.log")
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
        commands = ["boot", "test", "echo", "quit"]
        if lab >= 2:
            commands += ["mem", "vm"]
        if lab >= 3:
            commands += ["ticks", "irq"]
        if lab >= 4:
            commands += ["user", "utest", "proc"]
        if lab >= 5:
            commands += ["heap", "htest"]
        require(all(name in help_reply for name in commands),
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

        if lab >= 3:
            require("[test] trap PASS" in tests, f"trap register-restore probe missing:\n{tests}")
            first_ticks = timer_snapshot(machine.command("ticks"), cpus)
            machine.drain_for(0.35)
            second_ticks = timer_snapshot(machine.command("ticks"), cpus)
            require(all(second_ticks[hart] > first_ticks[hart] for hart in first_ticks),
                    f"timer did not advance on every hart: {first_ticks} -> {second_ticks}")
            before_irq = irq_snapshot(machine.command("irq"))
            passed.append("SBI timer interrupts advance on every active hart")

        if lab >= 4:
            check_user_processes(machine)
            passed.append("actual U execution+register/call/timer proofs; six contained protection faults; rollback and repeated page balance")

        if lab >= 5:
            check_user_heap(machine)
            passed.append("actual U heap allocation/write/read/free; real split/merge/reuse maps; freed/NX faults; rollback and page/node balance")

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

        if lab >= 3:
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
    parser.add_argument("--lab", choices=("1", "2", "3", "4", "5", "all"), default="all")
    parser.add_argument("--cpus", default="1,2,3,8", help="comma-separated CPU counts from 1 to 8")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-riscv64"))
    parser.add_argument("--toolprefix", default=os.environ.get("TOOLPREFIX", "riscv64-linux-gnu-"))
    parser.add_argument("--no-build", action="store_true", help="exercise existing build outputs")
    parser.add_argument("--evidence", type=Path,
                        help="optional evidence root with labN/cpuN/ subdirectories; defaults to each lab's docs/evidence/")
    args = parser.parse_args()
    try:
        cpus = list(dict.fromkeys(int(count) for count in args.cpus.split(",")))
    except ValueError:
        parser.error("--cpus must contain comma-separated integers")
    if not cpus or any(count < 1 or count > 8 for count in cpus):
        parser.error("CPU counts must be from 1 to 8")
    labs = [1, 2, 3, 4, 5] if args.lab == "all" else [int(args.lab)]
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    failures = 0
    for lab in labs:
        directory = (args.evidence / f"lab{lab}" if args.evidence else
                     ROOT / f"lab{lab}" / "docs" / "evidence")
        directory.mkdir(parents=True, exist_ok=True)
        lab_summary = [f"Automated Lab {lab} runtime verification. Started at {started}",
                       f"Independent sources: lab{lab}/.",
                       "Monitor is S-mode kernel code, not a user shell.", ""]
        lab_failures = 0
        for count in cpus:
            config_directory = directory / f"cpu{count}"
            config_directory.mkdir(parents=True, exist_ok=True)
            name = f"Lab {lab}, CPUs={count}"
            print(f"Checking {name} ...", flush=True)
            try:
                kernel = ROOT / f"lab{lab}" / "build" / f"cpu{count}" / "kernel.elf"
                if not args.no_build:
                    kernel = build(lab, count, config_directory)
                require(kernel.is_file(), f"kernel missing: {kernel}")
                check_elf(kernel, lab, args.toolprefix, config_directory)
                checks = exercise(lab, count, args.qemu, kernel, config_directory)
                records = [f"PASS {name}"] + ["  - " + check for check in checks]
                lab_summary.extend(records)
                print(f"PASS {name} ({len(checks)} groups)", flush=True)
            except (CheckFailure, OSError, subprocess.TimeoutExpired) as error:
                failures += 1
                lab_failures += 1
                lab_summary.append(f"FAIL {name}: {error}")
                print(f"FAIL {name}: {error}", flush=True)
        lab_summary.extend(["", f"Results: {len(cpus) - lab_failures} passed, {lab_failures} failed."])
        (directory / "summary.txt").write_text("\n".join(lab_summary) + "\n", encoding="utf-8")
        print(f"Lab {lab} evidence: {directory / 'summary.txt'}", flush=True)
    print(f"Results: {len(labs) * len(cpus) - failures} passed, {failures} failed.", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

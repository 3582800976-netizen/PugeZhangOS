# Lab-1 Work Log

> Historical record: the entries below describe the earlier xv6-based prototype.
> For the independent Lab 1–3 kernel implemented on 2026-10-05, see
> [the current design](docs/LAB123_DESIGN.md) and [runtime evidence](docs/evidence/lab123/summary.txt).

This log records the Lab-1 startup-chain work and decisions made so far. It
distinguishes repository changes and command-based verification from results
reported by the user.

## 2026-09-24 — Startup-chain analysis

- Reviewed the linker entry, `_entry`, `start()`, `mret`, and `main()` path,
  along with multicore startup, `started`, memory barriers, and the
  `printf`/console/UART/spinlock output path.
- Confirmed that the existing xv6 startup architecture already implements
  the core startup flow; the proposed educational extension was focused on
  making its stages observable rather than restructuring startup.
- At that time, the working tree contained changes in `kernel/entry.S`,
  `kernel/start.c`, and `kernel/main.c` adding an initial `boot_trace_state`
  snapshot. The analysis identified that this was a latest-state snapshot,
  not a complete event history.
- No files were changed during that analysis.

## 2026-09-24 — Lab-1 checkpoint commit

- Reviewed the changes and committed only:
  - `kernel/entry.S`: exported `_entry`.
  - `kernel/start.c`: recorded per-hart arrival at `start()`.
  - `kernel/main.c`: recorded arrival at `main()` and printed a boot-state
    snapshot; removed the former one-line secondary-hart startup print.
- Commit: `0e66d84d16c6fa47502502585abb0bedd0aae673`
- Commit message: `checkpoint: complete Lab-1 boot trace`
- No new Hart Trace or UART lock/no-lock experiment was implemented in this
  checkpoint.

## 2026-09-27 — Checkpoint push

- Pushed `main` to `origin` at
  `git@github.com:3582800976-netizen/PugeZhangOS.git`.
- The push updated the remote from `2b7746f` to `0e66d84`.
- A later check reported `Everything up-to-date` and a clean tracked worktree.

## 2026-09-28 — Baseline results reported by user

- `CPUS=1`: `hart0=2`, other harts `0`; xv6 started normally.
- `CPUS=3`: `hart0=2`, `hart1=2`, `hart2=2`; xv6 started normally.
- These are user-reported runtime results; they were not rerun as part of the
  subsequent read-only design analysis.
- Interpretation: the current value records each hart's latest observed
  stage (`2` for `main()`), not the full sequence of startup events.

## 2026-09-28 — Boot Trace v2 design (not implemented)

Recommended per-hart events:

1. `ENTRY` — `kernel/entry.S`, after stack setup and before `call start`.
2. `START` — beginning of `start()` in `kernel/start.c`.
3. `MRET_READY` — immediately before `mret`, after the M-to-S return state is
   configured.
4. `MAIN` — beginning of `main()` in `kernel/main.c`.
5. `GLOBAL_INIT_BEGIN` / `GLOBAL_INIT_DONE` — around hart 0's global
   initialization; completion should be recorded before publishing `started`.
6. `WAIT_STARTED` / `RELEASED` — for nonzero harts, before waiting and after
   the wait plus the existing memory barrier.
7. `LOCAL_INIT_DONE` — after each hart's local initialization, including
   `plicinithart()`.
8. `SCHEDULER` — immediately before entering `scheduler()`.

Design constraints:

- Accumulate event bits per hart instead of overwriting one scalar stage.
- Each hart writes only its own trace slot; recording does not need a lock.
- Keep `started` and its existing memory barriers as the actual startup
  synchronization; trace data must not control release or initialization.
- Do not call `printf` from `_entry` or early `start()`, before console and
  printf initialization. Print the trace only after those are available.
- Existing `printf` locking protects complete `printf` calls. The UART
  transmit lock protects the asynchronous transmit ring buffer and is not a
  substitute for printf's lock; synchronous `printf` output uses
  `uartputc_sync()`.
- No changes to `kernel/kernel.ld` or xv6's startup architecture are needed.
- Expected shape: with `CPUS=1`, only hart 0's path appears; with `CPUS=3`,
  harts 1 and 2 also show wait/release and local-init events. Relative ordering
  between different harts is nondeterministic; ordering within one hart should
  follow the startup sequence.

## 2026-09-28 — Work-log creation

- Added this file to preserve the preceding analysis, user-reported baseline,
  checkpoint history, and the unimplemented Boot Trace v2 design.
- At creation time, `git status` showed an existing untracked `shell/`
  directory. It was not inspected, modified, or removed.

## 2026-09-28 — Boot Trace v2 implemented and committed

- Implemented the bitmask-based per-hart boot trace described above.
- `kernel/entry.S`: added `boot_trace_entry()` call after stack setup.
- `kernel/start.c`: added `boot_trace_start()` / `boot_trace_mret_ready()`
  calls; replaced the former inline scalar `boot_trace_state` recording.
- `kernel/main.c`: event marks for `MAIN`, `GLOBAL_INIT_BEGIN/DONE`,
  `WAIT_STARTED`, `RELEASED`, `LOCAL_INIT_DONE`, `SCHEDULER`; prints each
  hart's event chain and milestone set before `scheduler()`.
- Commit: `b74b010` — `lab1: complete boot trace v2`.

## 2026-09-30 — Concurrent UART output experiment (locked vs unlocked)

- Goal: make the effect of `printf()`'s `pr.lock` visible under concurrent
  multi-hart output.
- Added a per-hart loop in `kernel/main.c` immediately before `scheduler()`:
  `for (int i = 0; i < 20; i++) printf("H%d-%d\n", id, i);`
  (uncommitted; see `git diff kernel/main.c`).
- Locked run (default `pr.lock` enabled, `CPUS=3`): each `H%d-%d` line printed
  intact; hart ordering interleaved but no character-level interleaving.
- Unlocked run (temporarily commented `acquire`/`release` in `printf.c`):
  character-level interleaving and corrupted output observed, e.g.
  `H1-189`, `H22`, `H2-111`, and garbled boot-trace lines.
- `kernel/printf.c` was restored to its original locked form and rebuilt.
- Conclusion: `uartputc_sync()` serializes a single byte but not a whole
  `printf`; atomicity of a complete `printf` comes from `pr.lock`.

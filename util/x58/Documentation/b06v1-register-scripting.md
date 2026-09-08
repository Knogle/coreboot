> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V1 transactional register scripting

Status: implemented, host-tested, built and statically inspected on 2026-09-01;
flashed and exercised in one boot of unclassified cold/warm provenance on
2026-09-04. Repeated cold-boot validation is still outstanding.

B06V1 extends the terminal pre-DRAM B06V0 CAR ROMMON with a bounded register
table interpreter. Its purpose is to make one hardware hypothesis an explicit,
reviewable sequence that can be loaded and repeated without rebuilding the ROM.
It is not a general-purpose language, does not allocate memory, and executes no
script automatically.

The engine supports at most 32 operations over port I/O, PCI configuration,
linear/MMIO addresses, and MSRs. A table must be sealed, its complete digest
must be echoed by the operator, and a separate one-shot `SCRIPT` arm must be
consumed before execution. Every attempted operation is emitted to COM1 before
the access and the UART is drained to TEMT. A surviving operation then emits a
POST record. This makes the last PRE line useful when an access resets or hangs
the machine.

## Source language

All numeric tokens are hexadecimal, with an optional `0x` prefix. Whitespace is
not significant and `#` starts a comment in host-side `.xrs` files.

| Operation | Source form | Meaning |
|---|---|---|
| read | `read SPACE TARGET WIDTH` | Read once and record the observed value. |
| write | `write SPACE TARGET WIDTH VALUE rev|nr` | Save the old value, write `VALUE`, and record one read-back. |
| mask | `mask SPACE TARGET WIDTH CLEAR SET rev|nr` | Save the old value, write `(old & ~CLEAR) | SET`, and record one read-back. |
| poll | `poll SPACE TARGET WIDTH MASK EXPECT LIMIT` | Perform at most `LIMIT` reads until `(value & MASK) == EXPECT`. |
| delay | `delay ITER` | Execute exactly `ITER` processor `pause` iterations. |
| assert | `assert SPACE TARGET WIDTH MASK EXPECT` | Read once and require `(value & MASK) == EXPECT`. |

For overlapping bits, `SET` wins; a normal field replacement therefore uses
the complete field mask in `CLEAR` and the desired field value in `SET`. An
assert or poll expectation may not contain bits outside its mask. A write
read-back is telemetry, not an implicit equality assertion: append an explicit
`assert` or `poll` when the hypothesis requires a particular resulting value.

Spaces and widths are:

| SPACE | TARGET | Width |
|---|---|---|
| `io` | 16-bit port number | `b`, `w`, or `l` |
| `pci` | packed `BB DD FF RR`, for example `ff020180` | aligned `b`, `w`, or `l` |
| `mem` | aligned 32-bit linear/MMIO address | `b`, `w`, or `l` |
| `msr` | 32-bit MSR index | `q` |

`b`, `w`, `l`, and `q` mean 1, 2, 4, and 8 bytes. A `q` value is written as
`HI:LO`, with two 32-bit halves. The maximum poll count is decimal 1,000,000
(`0x000f4240`), and the maximum delay is `0x01000000` pause iterations. A delay
is deliberately not advertised as microseconds: its duration depends on the
running CPU and current platform state. Polling is a tight bounded loop without
an implicit delay.

The interpreter validates widths, value ranges, PCI device/function numbers,
alignment, count limits, masks, and table length before accepting an operation.
The platform backend additionally rejects a scripted `mem` write that overlaps
the complete CAR window `0xfff80000..0xfff8ffff`; this protects the monitor,
stack, operation table, and transaction log. Ordinary MMIO, I/O, PCI, and MSR
accesses can nevertheless fault, reset, stop the CPU, destroy QPI, or make COM1
unreachable. There is no exception handler that can recover from such an
access.

## Loading and sealing a table

The ROMMON commands corresponding to the source forms have a `script add`
prefix. For example:

```text
script clear
script add read pci ff020180 l
script add assert pci ff020180 l ffffffff 030f0f03
script seal
script list
script status
```

`script seal` is a real state transition, not just a print command. Once
sealed, no operation can be appended; `script clear` is required to start a
different table. Execution of an unsealed table is rejected. `script status`
prints `SEALED=01` and an eight-digit `PROGRAM_FNV` for a valid sealed table.

The program digest is FNV-1a-32 over a versioned canonical representation of
every explicit operation field. It detects transcription errors and binds
`run` to the reviewed table, but it is not a cryptographic signature and is
not collision-resistant against a malicious author.

The host compiler validates the same rules, calculates the same canonical
digest, and emits only load/review commands:

```bash
python3 scripts/x58_rommon_script.py validate \
  research/scripts/b06v1-passive-state.xrs

python3 scripts/x58_rommon_script.py compile \
  research/scripts/b06v1-passive-state.xrs
```

The compiler intentionally never emits `unlock SCRIPT`, `script run`, a
rollback, or a reset. Paste its standard output into ROMMON, compare the final
ROMMON `PROGRAM_FNV` with the compiler result, and review every `script list`
line before arming execution.

COM1 uses 115200 8N1 without hardware or software flow control. Do not send the
complete compiler output as an unpaced byte stream: every `add` command prints
status before ROMMON accepts the next line, so a sender can overrun the UART
receive FIFO. Send one line only after the next `rommon>` prompt, or configure
the terminal/uploader with a conservative line delay. A dropped or altered
line must result in a different operation count or `PROGRAM_FNV`; never work
around that mismatch.

The supplied passive example has seven operations and canonical digest:

```text
PROGRAM_FNV=f9482511
```

It reads and asserts the observed Slow-QPI/ratio-6 tuple
`ff:02.1:80=030f0f03`, `ff:03.4:50=0a000006`, and
`ff:03.4:54=00000006`, then performs a short pause. Those exact values are a
gate for the pinned laboratory state, not a general X58 specification.

## Running and retaining the transaction

Execution always consumes a fresh one-shot arm:

```text
unlock SCRIPT
script run PROGRAM_FNV keep
```

or:

```text
unlock SCRIPT
script run PROGRAM_FNV auto
```

`keep` retains all mutations after success or failure. `auto` means
rollback-on-failure only; it does not roll a successful script back. Before
starting, `auto` rejects the entire table if any `write` or `mask` is marked
`nr`. No access is performed on an empty, unsealed, digest-mismatched, already
active, or auto/non-reversible table.

Every started run creates one retained transaction, including a read-only run
or a failed assert. While it exists, the program cannot be edited or executed
again. Inspect it with:

```text
script status
script trace
script trace START COUNT
```

Each trace records the exact operation, status, pre-write value, observed
post-access value, poll count, whether a write actually occurred, rollback
status, and rollback read-back. `script status` reports:

```text
FORMAT OPS PROGRAM_FNV SEALED TXN_VALID TRACE TXN_FNV LAST MUT NONREV
RB_AVAILABLE RB_RESULT RB_ATTEMPTED RB_TRIES ROLLED_BACK
```

`LAST` is the original execution result. This remains visible after an
automatic rollback, while `RB_RESULT` describes the restoration attempt.
`TXN_FNV` binds the runtime result and all recorded observations. It changes
when rollback state changes, so always copy the current value from
`script status` rather than reusing an older one.

The transaction must eventually be closed explicitly. For a read-only run or
when retaining the resulting state is intentional:

```text
unlock SCRIPT
script discard TXN_FNV
```

Discarding loses the saved pre-write values and therefore permanently removes
the monitor's rollback option for that transaction. Archive `script trace`
first.

Any run that actually starts also marks the separate B06V0 vendor-call path
dirty. CSI/MINIT operations are then blocked until a cold reset. A rejected
script preflight performs no access and does not dirty that path.

## Rollback contract

Only a `write` or `mask` explicitly marked `rev` participates in rollback.
Before the mutation the engine reads and stores the complete old value. A
manual or automatic rollback visits performed mutations in reverse order,
writes each old value, reads it back, and compares every bit in the selected
width.

Manual rollback also consumes a new arm and the exact current transaction
digest:

```text
unlock SCRIPT
script rollback TXN_FNV
```

If every restoration and serial event succeeds, `ROLLED_BACK=01`,
`RB_AVAILABLE=00`, and the transaction remains available only for inspection
and discard. If a restoration or its mandatory PRE/POST logging fails,
`ROLLED_BACK=00`; the old values remain in CAR, `RB_AVAILABLE=01`, and the
operator may retry using the newly printed `TXN_FNV`. `RB_TRIES` makes repeated
attempts explicit. Each retry again needs `unlock SCRIPT`.

`rev` is an operator assertion, not a hardware property discovered by ROMMON.
Rollback cannot reliably undo:

- write-one-to-clear, read-clear, write-once, lock, trigger, or self-clearing
  fields;
- commands already delivered to DDR3, PHY, QPI, reset, watchdog, or DMA
  machinery;
- dependent state changed by hardware after the original write;
- a reset, machine check, HLT loop, stopped UART, invalid CAR, or lost link;
- ordering requirements not represented by the exact reverse replay.

Mark such mutations `nr` and use `keep`, or do not place them in a script.
Never use `rev` merely to make `auto` accept an operation. The serial log and a
socketed recovery image remain the real recovery boundary.

## Serial and POST protocol

For each operation, B06V1 emits a line similar to:

```text
[SCRIPT] PRE I=00 OP=read SPACE=pci TARGET=ff020180 WIDTH=l
[SCRIPT] POST I=00 STATUS=done BEFORE=00000000 OBS=030f0f03 ...
```

The PRE line is drained to UART TEMT before the hardware access. If the target
hangs, the absence of a matching POST line identifies the boundary. The
B06V1-specific POST transitions are:

| POST | Meaning |
|---:|---|
| `e5` | script execution / operation PRE |
| `e6` | `run` returned an error |
| `e7` | `run` returned success |
| `e8` | rollback / restoration PRE |
| `e9` | manual rollback returned success |
| `ea` | manual rollback returned an error |

After a surviving command, the ordinary ROMMON loop returns to ready code
`bc`. Automatic rollback occurs inside `run`, so its final run result can be
`e6` even when `RB_RESULT=ok` and `ROLLED_BACK=01`; the original failed assert
or timeout remains the correct execution result.

## First hardware procedure

Use the exact B06V0 target hardware and recovery prerequisites. The first
B06V1 boot is passive: after true AC removal capture the full boot log and run
only `id`, `resetcause`, `spd`, `vinfo`, and `script status`. Confirm the B06V1
ID, microcode `0x1f`, the existing exact SPD/topology gates, intact CAR guards,
and no automatic script or vendor call.

On a separate cold boot, load only
`research/scripts/b06v1-passive-state.xrs`, seal it, and compare both copies of
`PROGRAM_FNV=f9482511`. Review `script list`, then execute:

```text
unlock SCRIPT
script run f9482511 keep
script status
script trace
unlock SCRIPT
script discard TXN_FNV
```

Expected success is seven `done` records, zero mutations, no rollback attempt,
and a return to the monitor. An assert mismatch is useful evidence and must not
be bypassed. No write/mask or rollback test is authorized by this passive
procedure; select a documented reversible register only after reviewing that
log as a separate hardware hypothesis.

Capture the complete console from power-on through transaction discard. If a
PRE line has no POST line, or COM1/POST stops responding, perform the established
external cold-power recovery rather than assuming rollback ran.

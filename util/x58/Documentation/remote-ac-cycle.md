> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Remote AC-cycle recovery with a Shelly Gen2 relay

Status: host-side helper and dry-run tests pass. Four retained live cycles had
completed by 2026-09-05: two with 15 seconds and two with 60 seconds of relay
off-time. The Shelly restored AC each time, but the board remained in S5 and
required a physical power-button press even though `GEN_PMCON_3[0]` had read
back clear. The relay is therefore a valid remote G3 method, but **not an
autonomous restart method** for the current board state.

## Three independent pieces of state

The ICH10R after-power-failure policy is not the experimental CMOS cookie.
Keeping these states separate is essential:

1. ICH10R device `00:1f.0`, PCI configuration byte `GEN_PMCON_3` at `0xa4`,
   bit 0 is named `SLEEP_AFTER_POWER_FAIL` by coreboot.  The local
   `i82801jx_power_options()` implementation documents and programs bit 0 as:

   ```text
   0 = enter S0 / full on when power returns after G3
   1 = remain in S5 / soft off when power returns after G3
   ```

   This is an ICH10 power-management register, not standard CMOS byte `0x0e`.
   Retained X58 Pro-E live logs include the complete word as
   `GEN_PMCON_3=0202`, whose bit 0 is already clear. Re-read the current boot
   before deciding that a write is necessary; an earlier `0206` statement in
   this document was not supported by the retained raw captures.

2. Standard CMOS byte `0x0e` is used by the experimental automatic firmware
   as an independent reset-loop authorization:

   | Image | Cold authorization | In progress | Failure lock |
   |---|---:|---:|---:|
   | B06V6/B06V7 | `6c` | `ec` | none |
   | B06V8--B06VA | `2c` | `ec` | `ed` |
   | B06VB--B06VL | `2c` | `ec` | persistent I801 `08:64:9b:5c:a3`; fail-closed fallback |

   B06V8 is retired: its alleged extended-CMOS tuple was actually standard
   RTC clock/calendar data because U128E was clear. B06V9 uses the same phase
   cookie but enables/exact-gates U128E and does not derive CSI state from RTC
   contents. The Shelly helper neither reads nor changes any CMOS byte.

3. Five idle I801 host registers contain volatile pass signatures.  They
   distinguish reset-separated CSI phases; they are not the after-G3 policy
   and must never be replaced by an unconditional retry counter.

## Verify or enable automatic power return in ROMMON

At a terminal CAR ROMMON prompt, first capture the read-only report:

```text
resetcause
pci 0 1f 0 a4 b
```

An even byte has bit 0 clear and already requests S0 after G3. For the
retained value `02`, perform no write.

Only if the current byte has bit 0 set, the reviewed register table
[`ich10-power-on-after-g3.xrs`](../research/scripts/ich10-power-on-after-g3.xrs)
clears that bit, preserves the other seven bits, and asserts exact read-back.
Generate the loading commands locally:

```bash
python3 scripts/x58_rommon_script.py compile \
  research/scripts/ich10-power-on-after-g3.xrs
```

Its canonical result is:

```text
PROGRAM_FNV=9a4df20b OPS=03 AUTO_ROLLBACK=YES
```

After reviewing `script list`, execute it deliberately in `keep` mode:

```text
unlock SCRIPT
script run 9a4df20b keep
script status
script trace
pci 0 1f 0 a4 b
```

Archive the trace.  Once bit 0 is verified clear, discard the retained
transaction with a fresh `unlock SCRIPT` and the exact current `TXN_FNV`.
Running a table marks the current ROMMON vendor path dirty; this procedure is
therefore for a terminal recovery state immediately before AC removal, not
between automatic CSI phases.  Coreboot routinely uses a byte read/modify/
write on this register, but retention and automatic startup on this exact
board still require the one controlled hardware validation described below.

## Clear the correct experimental guard before AC removal

If an automatic attempt left the guard at its in-progress or failure value,
use the firmware's dedicated helper rather than raw port `0x70/0x71` writes:

```text
unlock RESET
autoguard clear
```

Require the image-specific serial confirmation:

- B06V6/B06V7: `0xec -> 0x6c`;
- B06V8/B06V9/B06VA/B06VB: `0xec` or `0xed -> 0x2c`.

If the guard is already at the exact cold authorization, do not issue the
command.  A successful guard clear authorizes a future cold attempt but does
not itself create G3 and does not clear the I801 registers.  Conversely,
clearing `GEN_PMCON_3[0]` enables power return but does not authorize another
CSI attempt.  Preserve the serial log before disconnecting.

## Shelly helper

[`shelly_gen2_rpc.sh`](../scripts/shelly_gen2_rpc.sh) accepts an explicit host
and defaults to fail-closed behavior.  `status` is the only command that
contacts a Shelly without `--apply`:

```bash
scripts/shelly_gen2_rpc.sh --host 192.0.2.50 status
```

A cycle without `--apply` only prints the exact RPC URL:

```bash
scripts/shelly_gen2_rpc.sh --host 192.0.2.50 \
  --off-seconds 15 cycle
```

After the ROMMON preparation above and a first physical validation, explicitly
send one cycle request:

```bash
scripts/shelly_gen2_rpc.sh --host 192.0.2.50 \
  --off-seconds 15 --apply cycle
```

The helper first requires `Switch.GetStatus` to report that `switch:0` is on.
It then sends one request equivalent to:

```text
/rpc/Switch.Set?id=0&on=false&toggle_after=15
```

The return timer runs in the Shelly, so loss of the AP or management path
after the relay opens does not prevent the already-armed relay close.  The
script deliberately does not implement a fragile host-side `off; sleep; on`
sequence.  It also does not reconnect the serial console, clear a firmware
guard, infer a successful boot, or repeat a failed cycle automatically.

Plain `off` can strand the target and therefore needs two explicit gates:

```bash
scripts/shelly_gen2_rpc.sh --host 192.0.2.50 \
  --apply --allow-stay-off off
```

## Controlled validation and observed result

Use a known-good image for the first relay test and record the complete power
and serial timeline:

1. Require current `GEN_PMCON_3[0]=0` and the correct image-specific CMOS
   authorization.  Do not modify either while an automatic pass is active.
2. Confirm the Shelly controls only the target AC load and remains powered
   when its relay opens.  If the same load includes the AP or ConsolePi, expect
   management connectivity to disappear temporarily.
3. Arm a 15-second local-timer cycle.  Confirm the board actually loses
   standby power; elapsed time or Shelly `apower` alone does not prove that
   motherboard rails have discharged.
4. After power returns, require a new reset-vector banner, `COLD_DEFAULT`, the
   exact cold authorization, and no stale B06V8/B06V9 I801 signature. A CF9 reset
   or CSI/SYRE CPU-only reset does not satisfy this check.
5. Repeat only after inspecting the entire preceding log.  One success is not
   the project's ten-cold-boot validation milestone.

An HTTP timeout is ambiguous: the Shelly may have accepted the state-changing
request before the response was lost.  Do not blindly issue a second cycle;
wait for the configured local timer and query status after connectivity
returns.  If AC returns but the board remains in S5, the after-G3 setting did
not survive or the board did not reach true G3.  A separate physical power-
button/recovery path is then still required.

That final failure mode occurred in the first live B06V9 cycle on 2026-09-04
and in three later B06VK cycles on 2026-09-05. `00:1f.0:a4` had read `02`; the
Shelly's local timer restored its output after 15 or 60 seconds, but no new
reset-vector banner appeared until a physical-button start. Do not schedule
another unattended cycle until a separate board-specific power-on mechanism
has been proven.

The helper regression test uses a local fake `curl` and opens no socket:

```bash
scripts/test_shelly_gen2_rpc.sh
```

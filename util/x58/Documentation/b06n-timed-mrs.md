> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06N timed-MRS one-channel baseinit ROMMON

B06N is the corrected first hardware candidate for automating the base state
already reached manually during B06J-HW-01.  It preserves every B06M gate and
write while adding explicit, bounded UART-TEMT timing after each reset/CKE
phase marker and after every DDR3 MRS command.

Nothing new runs at boot.  The board still stops in the CAR ROMMON at POST
`0xbc`; all writes require the exact one-operation arm:

```text
unlock WRITE
baseinit
```

## Hypothesis and timing change

The vendor disassembly waits after MRS.  B06M's C path instead performed only
an unmeasured serialized PCI read.  In B06N, each completed MRS-valid poll is
followed by transmission of `.` and a bounded wait for UART LSR TEMT.  At
115200 baud, 8N1, one ten-bit frame takes about 86.8 microseconds.  This is a
hardware-clocked lower bound after every command and exceeds the observed
vendor delay without relying on an unknown TSC frequency.

The expected successful section is:

```text
[BASE] PREFLIGHT_OK
[BASE] POLICY_PATTERN_OK
[BASE] IGNORE_RX
[BASE] MC_RESET=1
[BASE] DIMM_RESET=1
[BASE] BLOCK_CKE=1
[BASE] RESET_RELEASED
[BASE] CKE_ASSERTED
[BASE] MRS_GAPS=........
[BASE] MRS_RANKS_0_1_OK
[BASE] RCOMP_OK
[BASE] ZQCL_R0_OK
[BASE] ZQCL_R1_OK
[BASE] READY STATUS=........ QPI=030f0f03 INIT_DONE=0
```

Exactly eight dots prove that all four MRS commands on both ranks passed their
bounded controller poll and their UART TEMT gate.  They do not prove correct
DDR mode-register contents or usable memory; those remain hypotheses checked
indirectly by later training behavior.

## First hardware test

Use the socketed W25Q128 recovery setup, the exact E5645, the same sole
dual-rank `BLS4G3D1609DS1S00.` responder at SPD `0x54`, QPI Slow Mode and DDR
ratio 6.  Remove AC power completely before inserting or flashing the test
chip.  Connect COM1 at 115200 8N1 with no flow control.

Run one command at a time and wait for the prompt after each:

```text
id
unlock WRITE
baseinit
pci ff 06 0 7c l
pci ff 06 0 70 l
pci ff 06 0 74 l
pci ff 02 1 80 l
```

Expected invariants after `baseinit` are two ranks still present, QPI
`030f0f03` unchanged, and `MC_CONTROL.INIT_DONE` clear.  If any command fails,
the UART stops responding, or the POST board leaves the expected monitor/error
contract, do not retry in the same powered state.  Remove AC power and retain
the complete serial and POST trace.

Only if the isolated base state returns cleanly should the next powered test
run:

```text
unlock WRITE
rdexact 2
```

Do not run `rdsweep` before that point.  B06N does not access ordinary DRAM,
set `MC_CONTROL.INIT_DONE`, retrain QPI, execute ramstage or a payload, or write
SPI flash.  A failed experiment is recoverable by restoring the known-good
socketed flash chip after complete AC removal.

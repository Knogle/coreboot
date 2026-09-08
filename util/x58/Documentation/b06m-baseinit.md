> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06M one-channel baseinit and exact-RD ROMMON

B06M preserves the complete B06L CAR monitor and adds one opt-in command:

```text
unlock WRITE
baseinit
```

Nothing new runs automatically at boot.  `baseinit` consumes the one-operation
write arm and is deliberately limited to the exact E5645, Slow-QPI,
DDR-ratio-6, channel-2, dual-rank DIMM configuration used by B06J-HW-01.

## Hypothesis

The B06J live session established a complete pre-training base state manually:

- `MC_CHANNEL_MAPPER=0x00024489`, one-channel `MC_CONTROL=0x00000400`;
- channel-2 DIMM0 geometry `0x000002ac`, ranks `0x03`;
- the exact ratio-6 timing, ODT, scheduler, background and page-table values;
- six recovered pattern-generator buffers;
- DIMM reset/CKE, MRS2/MRS3/MRS1/MRS0 on both ranks;
- bounded RCOMP and ZQCL completion.

B06M automates only that already executed state.  The new experiment is whether
the same sequence remains deterministic when executed at firmware speed and
then feeds B06L's corrected per-point RD order.

## Preflight and bounded behavior

Before its first write, `baseinit` requires all of the following exactly:

```text
CPUID.1:EAX          = 000206c2
ff:02.1:80           = 030f0f03
ff:03.4:50           = 0a000006
ff:03.4:54           = 00000006
ff:03.0:48           = 00000000
ff:06.1:48           = 00000000
ff:06.0:58/70/74/7c  = 00000000
```

All policy writes have exact readbacks.  RCOMP, MRS-valid, ZQCL, PHY and
training polls are finite.  The reset transitions are separated by serialized
UART markers.  Static re-audit after this artifact was built found that B06M
does not explicitly wait for the final marker character to reach TEMT and its
MRS path has only a serialized PCI read as an unmeasured gap.  Therefore B06M
must not be used for the first hardware test; B06N is the corrected successor
with an explicit bounded TEMT wait after every MRS and reset-phase marker.

Expected successful phase log:

```text
[BASE] PREFLIGHT_OK
[BASE] POLICY_PATTERN_OK
[BASE] IGNORE_RX
[BASE] MC_RESET=1
[BASE] DIMM_RESET=1
[BASE] BLOCK_CKE=1
[BASE] RESET_RELEASED
[BASE] CKE_ASSERTED
[BASE] MRS_RANKS_0_1_OK
[BASE] RCOMP_OK
[BASE] ZQCL_R0_OK
[BASE] ZQCL_R1_OK
[BASE] READY STATUS=........ QPI=030f0f03 INIT_DONE=0
```

Any failure after `PREFLIGHT_OK` leaves partially initialized volatile state;
do not retry.  Remove AC power completely before the next attempt.

## Superseded hardware test

Do not flash B06M for the first automated-base-state test.  Use B06N and its
test procedure instead.  The command sequence below is retained only as the
historical B06M plan.

```text
id
unlock WRITE
baseinit
pci ff 06 0 7c l
pci ff 06 0 70 l
pci ff 06 0 74 l
pci ff 02 1 80 l
unlock WRITE
rdexact 2
```

Do not run `rdsweep` until isolated `rdexact 2` has returned to ROMMON with
QPI unchanged.  B06M does not set `MC_CONTROL.INIT_DONE`, access ordinary
DRAM, execute ramstage/payload, retrain QPI, or write SPI flash.

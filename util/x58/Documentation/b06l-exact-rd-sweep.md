> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06L exact coarse-RD sweep ROMMON

B06L preserves the B06K CAR ROM monitor and all existing diagnostic paths.  It
adds two opt-in commands; neither runs at boot and both require the exact
one-operation `unlock WRITE` arm:

```text
rdexact SWEEP
rdsweep
```

The commands are limited to CPU unit 0, channel 2, ranks 0/1 present, and the
eight non-ECC data lanes recovered from MSI `MINITDLL` V8.14B8.  They are not a
general DDR3 initialization implementation.

## Hypothesis

The B06J live sweep point at zero used the recovered PHY descriptor set but
inserted FIFO reset `0x20600` immediately before RD command `0x26b01`.  It
completed with status `0x100`; all A fields were zero and lane-specific B
fields ranged from `0x1d` through `0x22`.

Static reconstruction of MSI functions `fffcbe51`, `fffd2bbd`, and
`fffd5fc5` establishes a different order for every coarse-RD point:

1. seed the nine pulse descriptors, rank-wide sweep descriptor, and 16 lane
   result descriptors;
2. issue the documented MRS2/MRS3/MRS1/MRS0 series for each present rank;
3. issue `0x30200` after each rank's MRS series and wait for status bit 9;
4. issue bounded ZQCL for each present rank;
5. pulse the recovered width-7 PHY fields once;
6. issue rank-0 RD command `0x26b01` directly, with no intervening FIFO reset.

Later cross-checking against Intel DX58SO function `0xffe37125` showed that
Intel runs this sequence without the MSI-only bank-4 command.  MSI
`0xfffd2bbd` derives that extra command's low word from a runtime 15-row DIMM
table; the formerly assumed constant `0x3308` cannot be produced by the
observed lookup and is withdrawn.  The local source now defaults to the
Intel sequence and has no guessed bank-4 write.  B06L must nevertheless remain
an unflashed superseded artifact because it does not reproduce the complete
per-DIMM/per-lane PHY setup or the exact cold training state.  The ordering
above is static evidence, not a successful hardware sequence.

## Gates and timeouts

Before the first write, the exact path requires:

```text
ff:06.0:7c low byte = 03
ff:06.0:70          = 08061528
ff:06.0:74 low word = 0000
ff:02.1:80          = 030f0f03
```

These establish the already proven dual-rank MR policy and Slow-QPI state.
Every PHY busy wait, MRS-valid wait, ZQCL wait, and training wait is finite.
Every point prints status and all eight A/B lane pairs.  `rdsweep` walks the
vendor-observed even sequence `0x00..0x80`, stops at the first
`RD_DQ_DQS_PASS`, or prints `RD_SWEEP_NO_PASS`.

The MRS-valid clear poll is followed by another serialized CF8/CFC read.  The
pre-RAM CPU port has no calibrated `udelay()` implementation yet; this avoids
inventing a TSC frequency while providing a gap longer than DDR3 tMRD.

## First hardware test

Use the socketed recovery setup and perform a complete AC removal before the
run.  B06L itself does not reconstruct the visible DDR3-800 base registers,
DIMM reset/CKE, RCOMP, or initial ZQCL at boot.  Reapply and record that already
proven state first.  Then run only:

```text
id
pci ff 06 0 7c b
pci ff 06 0 70 l
pci ff 06 0 74 w
pci ff 02 1 80 l
unlock WRITE
rdexact 2
```

Only after that isolated point preserves QPI and returns to ROMMON should
`rdsweep` be attempted.  A timeout or UART fault requires complete AC removal.
No command writes SPI flash, sets `MC_CONTROL.INIT_DONE`, accesses ordinary
DRAM, or enters ramstage.

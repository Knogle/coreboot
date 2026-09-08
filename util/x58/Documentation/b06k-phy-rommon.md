> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06K PHY-aware CAR ROMMON

B06K retains B06J's cache-as-RAM monitor and adds bounded access to the X58
IMC PHY scan chain.  Boot remains passive: it reaches POST `0xbc` and waits at
`rommon>`.  No new IMC, QPI, DDR, or SPI write occurs until an exact
`unlock WRITE` plus a write command is entered.

The first hardware configuration is deliberately fixed to the Xeon E5645,
one Crucial `BLS4G3D1609DS1S00.` DIMM at SPD address `0x54`, inferred channel
2, rank 0, eight data lanes, and the already proven DDR3-800 visible-register
setup.  The profile commands are not valid for a different topology.

## Commands

All numeric arguments are hexadecimal.

```text
phyr CH START WIDTH
unlock WRITE
phyw CH START WIDTH VALUE MODE

unlock WRITE
rdtry SWEEP

unlock WRITE
rcvtry COARSE
```

`phyr` is logically read-only although the hardware interface requires writing
the channel selector and read command.  `phyw` accepts channels 0 through 2,
widths 2 through 30, a descriptor fully below last known field bit `0x1603`,
and mode 0 or 1.  Every busy wait is bounded.  A timeout is reported and no
retry is attempted.

`rdtry` is fixed to channel 2/rank 0.  It applies the MSI coarse-RD pulse and
lane seed descriptors, writes the requested rank-wide sweep value, executes
FIFO reset `0x20600`, then runs RD-only command `0x26b01`.  It prints
`RD_STATUS` followed by the two useful values for each data lane.  Allowed
sweep values are `0x00..0x80`; MSI's observed loop uses even values through
`0x80`.  The width-9 descriptor has seven useful payload bits, so the final
`0x80` attempt wraps to useful value zero exactly as MINIT's mask does.

`rcvtry` sets the observed channel-2 `MC_CHANNEL_ODT_PARAMS2` low word to
`0x0100`,
applies the MSI preliminary RCVEN descriptors, FIFO reset, and
receive-enable-only command `0x27301`, then prints all lane pairs.  Its coarse
input is limited to `0x00..0x3f`.

Both profiles wait for the command action bit to clear and status bit 8 to
assert.  This prevents the already-set completion value `0x100` from being
mistaken for completion of the newly issued command.

Neither profile sets `MC_CONTROL.INIT_DONE`, declares DRAM usable, performs a
memory access, or continues to ramstage.  A completion status containing only
`0x100` means the FSM completed but its requested pass bit is absent.

## First test contract

Use the socketed experimental chip and keep the known-good recovery chip.  Do
a complete AC removal before replacing or programming the flash.  Program and
read back the complete 16-MiB image, then verify its SHA-256 before power-on.

At the prompt, first run only:

```text
id
phyr 2 9fa 8
pci ff 2 1 80 l
```

Expected identity is `X58PROE-B06K-PHY-ROMMON-20260901`; QPI should remain in
the previously observed L0 tuple.  Only after those reads succeed should the
already proven visible DDR3-800 setup be applied.  Then begin with one
explicitly armed `rdtry 0`, capture the complete eight-lane result, and remove
AC power after the session.  Do not run an automated sweep until one isolated
profile has returned cleanly.

The descriptor derivation and live evidence are recorded in
[`research/msi/minit-phy-scan-chain-2026-09-01.md`](../research/msi/minit-phy-scan-chain-2026-09-01.md).

> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI CSI_INITDLL PCI-access reconstruction

## Evidence boundary

This analysis uses the locally extracted MSI `A7522IMS.8F0` `CSI_INITDLL` PE.
No proprietary bytes are checked in.  Its meaningful `.text` section is
29,174 bytes and has SHA-256:

```text
37b0fef95af7c176f2c7d310b67aee7b4e761cfa4173abe859d50313a9091a61
```

`scripts/analyze_csi_pci.py` is bound to that hash.  It recognizes the six
8/16/32-bit MMCONFIG helpers at `0xfffebc68..0xfffebe09` and performs only
conservative straight-line propagation of literal register constants.  It
clears all inferred constants at control-flow branches and preserves only the
i386 cdecl callee-saved registers across calls.  Regression tests cover those
rules and invalidate a 32-bit value after writes to any of its 8/16-bit
subregisters.

Reproduction:

```bash
python3 scripts/analyze_csi_pci.py \
  blobs-local/msi-x58-pro-e/extracted/CSI_INITDLL-region.bin \
  --only-literal-targets
```

The pinned image contains 376 calls to the helpers: 183 reads and 193 writes.
Device, function, and offset are statically recoverable at 53 call sites.
The bus remains dynamic at many sites, so a recovered `00.0`, for example,
must not automatically be called CPU Uncore or IOH.  Values assembled from
runtime policy fields also remain unresolved.

## Paired QPI evidence

Function `0xfffec262` writes the same runtime value to offset `0x80` of
device/function `02.0` and `02.4` on the runtime CPU bus.  These are the
published link-layer functions for CPU QPI links 0 and 1.  The value is not a
literal and its meaning is not assigned.

The same DLL also accesses device/function targets 16, 20, and 22.  Only the
following subset has a literal bus number; the others use a runtime bus field:

```text
literal bus 0: 14.1 +0x09c, +0x0a0, +0x0a8; 14.2 +0x0cc
runtime bus:   14.2 +0x080, +0x0d0, +0x0d4, +0x280, +0x284
runtime bus:   10.1 +0x040, +0x064, +0x0b8; 16.0 +0x060
```

At `0xfffec297`, MSI reads IOH `00:14.1 +0xa8` and then writes literal
`0x01840000` to it.  A read-only check of the currently vendor-initialized
reference machine on 2026-09-01 returned the same post-init value:

```text
00:14.1 +0x09c = 0xbf000000
00:14.1 +0x0a0 = 0x00000000
00:14.1 +0x0a8 = 0x01840000
00:14.2 +0x080 = 0x02a080a4
00:14.2 +0x0cc = 0x00004200
00:14.2 +0x0d0 = 0x00000000
00:14.2 +0x0d4 = 0x92080800
```

IOH functions `00:10.1` and `00:16.0` were not selectable after vendor POST,
consistent with the already documented `DEVHIDE1` state.  No hide bit or
other register was changed for this observation.

## Entry chronology and progress register

The DLL entry at `0xfffe7000` provides a stronger ordering anchor than the
flat helper-call inventory.  The following sequence is verified directly in
the MSI disassembly; names in the right column describe only observed effects:

| Entry address | Call/effect | Evidence boundary |
|---:|---|---|
| `0xfffe7021` | call `0xfffe9957` | constructs the local CSI context; its call to `0xfffec297` sets IOH `00:14.1:a8=01840000` |
| `0xfffe702c` | checkpoint `a0` | writes the code to the high byte of IOH `00:14.1:9c` and to a policy-selected I/O port |
| `0xfffe7031` | call `0xfffea8e6` | derives local topology/policy fields, including the CPU Uncore bus at local offset `+0x07` |
| `0xfffe703c` | checkpoint `a1` | policy derivation has returned |
| `0xfffe7042` | call `0xfffec262` | writes the current local CSI-context pointer to CPU-bus `02.0:80` and `02.4:80` |
| `0xfffe704d` | checkpoint `a2` | paired CPU-link write has returned |
| `0xfffe7054..715f` | further staged calls | checkpoints `a3`, `a4`, `aa` through `af`; semantics require separate reconstruction |
| `0xfffe716a` | checkpoint `ea` | final visible entry checkpoint before result/status handling, not proof of success by itself |

The caller at `0xfffc04e2` initializes CSI input byte `+0x04` to `0xff`.
Policy derivation at `0xfffea8e6` loads that byte, calls subtraction helper
`0xfffebfcf` with zero, and stores the unchanged `0xff` at local offset
`+0x07`.  In `0xfffec262`, that local byte is the **bus** argument.  Direct
inspection of write helper `0xfffebe09` establishes its cdecl signature as
`(context, bus, device, function, offset, value)`: the value in both calls is
the current local CSI-context pointer held in `ESI`, not the bus byte.  This
corrects the result one would get by treating the pushes without first
validating the callee's stack layout.

CPU link-layer offset `02.0/02.4:80` is not named in the public register set
used by this project.  Why CSI temporarily publishes its CAR context pointer
there is not yet established.  The later post-init value must not be treated
as either that pointer or its input policy.

Checkpoint helper `0xfffec217` writes its code into bits 31:24 of
`00:14.1:9c`, replacing the other 24 bits with zero.  Related helpers update
the lower three bytes.  A fresh read-only reference check returned:

```text
00:14.1 +0x09c = 0xbf000000
ff:02.0 +0x080 = 0x0000fe0d
ff:02.4 +0x080 = 0x0000fe0d
ff:02.1 +0x080 = 0x070f0f03
```

`0xbf` is therefore consistent with a later completed CSI progress stage, but
the entry function shown above does not itself establish the meaning of `bf`.
The identical link-0/link-1 `0xfe0d` values show that the early context-pointer
publication was overwritten before the running-system capture.  They are not
evidence for a pointer encoding, a field decode, or a safe replay recipe.

The simultaneous CPU-link and IOH-internal access clusters support a paired
QPI initialization protocol.  They do not establish a safe replay order,
field meanings, reset requirements, or a callable CSI ABI.  In particular,
they do not justify copying the vendor High-Speed terminal values into B06M.
The current hardware sequence therefore remains: establish repeatable DDR
training while the autonomous Slow-QPI link is stable, then introduce QPI
work as a separate recoverable experiment.

## Caller QPI-ratio policy byte

The caller's byte `+0x07` is distinct from local CSI byte `+0x07`.  Function
`0xfffe7d85` reads caller `+0x07` and decodes three explicit selections:

| Caller `+0x07` | Internal ratio byte | Public-register interpretation |
|---:|---:|---|
| `0x01` | `0x12` | QPI PLL ratio 18 |
| `0x02` | `0x16` | QPI PLL ratio 22 |
| `0x03` | `0x18` | QPI PLL ratio 24 |

The internal byte is later stored at local `+0x136`.  The Intel Xeon 5500
datasheet names CPU `02.1/02.5:54[6:0]` `NEXT_PLL_RATIO` and states that the
ratio is multiplied by 133 MHz.  The three literal values therefore identify
caller `+0x07` as a QPI operational-ratio policy selector.  This does not yet
establish all special encodings: `0x06` bypasses the body of
`0xfffe7d85`, while `0x20` follows a separate auto/detected-policy path.

The MSI wrapper fallback at `0xfffc0c0c` reads byte
`0xeff11053`, which decodes under its programmed `0xe0000000` PCIEXBAR as CPU
`ff:02.1:53`, the high byte of `QPI_0_PLL_STATUS`.  After masking to five
bits, maximum-ratio values `0x12`, `0x16`, and at least `0x18` are converted
to selectors 1, 2, and 3 respectively.  This is a capability fallback, not a
read of the current ratio.  The selected setup/NVRAM policy can override it
before CSI is called.

The downstream use is now bounded more tightly as well:

- discovery function `0xfffe756a` tests the existing link-up identifier at
  physical-status offset `0x80` before marking a local link structure active;
- `0xfffe7d21` iterates the discovered links and calls `0xfffeae91`, which
  writes the selected seven-bit ratio to physical offset `0x54`;
- `0xfffead1d` performs a read/modify/write at physical-control offset `0x6c`
  and sets documented `LINK_SPEED` bit 7;
- `0xfffe8a7f` applies the caller's per-link scramble policy by calling
  `0xfffead63` or `0xfffeadd3`, which set or clear documented
  `ENABLE_SCRAMBLE` bit 22 at the same control register;
- `0xfffeb0e3` sets documented `DISABLE_AUTO_COMP` bit 13;
- later code updates the peer-side link structure and only then reaches the
  final trigger-oriented stage before checkpoint `af`.

These calls prove that MSI does not switch speed with one terminal-state
write.  They also explain why the already operational Slow-QPI link is useful
for bring-up: CSI explicitly discovers an existing link before applying its
requested operational-ratio policy.  The still-private peer-side structure
and trigger sequence prevent this partial reconstruction from being used as a
write recipe yet.

The vendor comparison captures make the operational distinction concrete:

```text
Slow setting: ff:02.1:50 = 160c0110, :54 = 00000010, :80 = 030f0f03
High setting: ff:02.1:50 = 160c0112, :54 = 00000012, :80 = 070f0f03
```

Thus the tested MSI "High" setting currently means ratio 18 (about 2.4 GHz
forwarded clock / 4.8 GT/s signaling), despite the E5645 reporting a maximum
ratio of 22 in `PLL_STATUS[30:24]`.  This observed terminal state is useful
for a later QPI experiment, but it is still not a safe initialization recipe:
the PHY-control ordering, reset transition, IOH peer policy, and all bounded
completion checks remain to be reconstructed first.

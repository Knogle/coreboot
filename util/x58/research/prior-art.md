> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Public X58/coreboot prior-art survey

Survey date: 2026-08-03.  The search covered coreboot Git history and Gerrit,
the coreboot mailing-list archive, public Git hosting, EDK2 platform code, and
older enthusiast reports.

## Result

No public implementation was found that initializes Bloomfield/Gulftown,
their integrated triple-channel DDR3 controller, CPU-to-X58 QPI, and the X58
IOH under coreboot.  The public record contains useful diagnostics and several
abandoned or exploratory efforts, but no port from which RAM/QPI code can be
adopted.

This is a negative search result, not proof that no private implementation has
ever existed.

## Coreboot commits often mistaken for X58 support

- [Commit 9702b6bf: add X58/ICH10R IDs to
  inteltool](https://github.com/coreboot/coreboot/commit/9702b6bf7ec5a4fb16934f1cf2724480e2460c89)
  changes a diagnostic utility only.
- [Commit 3235eea7: add X58 DMIBAR dumping to
  inteltool](https://github.com/coreboot/coreboot/commit/3235eea7289ab274d74052613b2cd55732565310)
  reads the northbridge register; it does not initialize it.
- The original mailing-list context is preserved in the
  [DMIBAR patch](https://www.mail-archive.com/coreboot%40coreboot.org/msg25928.html),
  [patch-series discussion](https://www.mail-archive.com/coreboot%40coreboot.org/msg25930.html),
  and [CPU-side PCIEXBAR/MCHBAR
  discussion](https://www.mail-archive.com/coreboot%40coreboot.org/msg25935.html).
- [Commit dbc6fcd0, initial Nehalem support in
  inteltool](https://github.com/coreboot/coreboot/commit/dbc6fcd021759280c71b0e246c0ede34f4879bac)
  is likewise runtime inspection, principally for a Lynnfield/Westmere-DMI
  endpoint, not an X58 boot path.
- [Commit 95de2317](https://github.com/coreboot/coreboot/commit/95de2317c6c6379e43d3b3c27d34eb66198dbe0a)
  and [Gerrit change 38941](https://review.coreboot.org/c/coreboot/+/38941)
  renamed the old `intel/nehalem` northbridge to `intel/ironlake`: it was
  Arrandale/Ironlake code.  It must not be presented as desktop Nehalem/X58.

The current reusable southbridge component is
[`src/southbridge/intel/i82801jx`](https://github.com/coreboot/coreboot/tree/main/src/southbridge/intel/i82801jx).
It covers useful ICH10 functions but does not supply CPU CAR, IMC training, or
QPI initialization.

## Public port attempts and discussions

- In [Trials on aftermarket X58
  motherboards](https://www.mail-archive.com/coreboot%40coreboot.org/msg56765.html)
  (2021), an experiment began by flashing an unrelated X201 image.  The
  response states that ICH10 exists but CPU, X58, and especially RAM
  initialization do not:
  [Angel Pons' reply](https://www.mail-archive.com/coreboot%40coreboot.org/msg56767.html).
  [SerialICE was suggested](https://www.mail-archive.com/coreboot%40coreboot.org/msg57043.html)
  as a reverse-engineering method.  No subsequent source or working milestone
  was found.
- A [Dell R610 Nehalem/Westmere-EP
  enquiry](https://www.mail-archive.com/coreboot%40coreboot.org/msg52927.html)
  likewise found no comparable QPI platform support.
- A [Sun Fire X4170 / Intel 5520
  enquiry](https://www.mail-archive.com/coreboot%40coreboot.org/msg24195.html)
  received the answer that support did not exist and was a substantial new
  development effort:
  [reply](https://www.mail-archive.com/coreboot%40coreboot.org/msg24197.html).
- The [ECS X58B-A2
  thread](https://www.mail-archive.com/coreboot%40coreboot.org/msg31679.html)
  contributed hardware and utility dumps, not a booting port.
- A broad [Gerrit X58 search](https://review.coreboot.org/q/X58) shows no X58
  platform patch series.  Searches for Bloomfield, Gulftown, Tylersburg,
  LGA1366, Nehalem-EP, Westmere-EP, DX58SO, and MS-7522 did not reveal one
  either.

An older German enthusiast
[PC Games Hardware project log](https://extreme.pcgameshardware.de/threads/projekttagebuch-coreboot-bios-crossflashing-uefi-auf-alten-mainboards.558706/)
records experiments and intentions around legacy boards/X58, but no published
coreboot X58 silicon implementation or reproducible RAM/QPI milestone was
identified.

## EDK2 Simics X58 is not hardware initialization

Tianocore publishes
[`BoardX58Ich10`](https://github.com/tianocore/edk2-platforms/tree/master/Platform/Intel/SimicsOpenBoardPkg/BoardX58Ich10)
and
[`SimicsX58SktPkg`](https://github.com/tianocore/edk2-platforms/tree/master/Silicon/Intel/SimicsX58SktPkg).
These target Intel's simulated `motherboard_x58_ich10`, not a physical DX58SO
or MSI board.

- The [pre-memory board-init
  library](https://github.com/tianocore/edk2-platforms/blob/master/Platform/Intel/SimicsOpenBoardPkg/BoardX58Ich10/Library/BoardInitLib/PeiX58Ich10InitPreMemLib.c)
  contains no physical DDR3/QPI bring-up equivalent.
- [MemDetect.c](https://github.com/tianocore/edk2-platforms/blob/master/Platform/Intel/SimicsOpenBoardPkg/SimicsPei/MemDetect.c)
  obtains simulated memory information rather than performing SPD discovery
  and electrical training.
- The [Intel Simics X58
  documentation](https://intel.github.io/simics/docs/rm-QSP-x86/index.single-page.html)
  confirms the virtual-platform context.

This code can later provide structural hints for ACPI, PCI, or an EDK2 payload,
but it does not reduce the physical pre-RAM problem.

## Engineering consequence

The first MSI X58 Pro-E work must be represented honestly as a new platform
port:

1. reuse generic x86 entry and Intel non-evict CAR where the MSI sequence
   proves compatibility;
2. reuse/audit ICH10R and common Fintek code;
3. add Bloomfield CPU/socket and X58 skeletons;
4. gather SPD and vendor-initialized CPU-Uncore/X58 state;
5. reconstruct a native, bounded QPI and one-DIMM DDR3 state machine;
6. launch SeaBIOS and EDK2 only after that platform state is complete.

Inteltool support, DMIBAR recognition, the historical `nehalem` directory, and
the Simics package must not be cited as evidence that X58 initialization is
already available.

## 2026-08-31 supplied overview assessment

A user-supplied German overview of public X58/5520 work was compared with this
survey before B05.  It supports the existing engineering split: reuse the
audited ICH10 pieces, while treating CPU Uncore/IMC, DDR3 training, and
CPU-to-IOH QPI as the new platform work.  It did not provide a complete,
board-verified initialization sequence or a callable public X58 memory-init
implementation.

The 5520 material is still useful later as terminology for bounded QPI-PHY
diagnostics and CPU-side SAD/TAD/channel register inventory.  Its dual-socket
topology, Node-ID policy, and register values must not be copied into the
single-socket X58 path without X58-specific documentation plus MSI/Intel
firmware and live-state correlation.  Therefore it did not broaden B05 beyond
the single I801 SPD type-byte transaction.

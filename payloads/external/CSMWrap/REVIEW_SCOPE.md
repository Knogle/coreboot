# CSMWrap review scope for the B565 V26 snapshot

The legacy boot implementation is a separate payload feature. It is outside
B565 DRAM initialization, PSP firmware layout, PCIe setup and the coreboot
board boot path. The V26 image includes it as an optional EDK2 application,
with SeaBIOS supplying Csm16.bin and the Compatibility16 services.

This maps the **existing** V26 patch stack. It does not claim that the stack
already builds or runs on another board. The source revision is
2dee6a2e3b3e7230082b9dcfc5a4097253c58cea. Earlier CSM entry tests were
performed on the B565D4-V1L; V26 cold/warm qualification exercised the UEFI
boot path with the CSM application present in the payload.

## Source ownership and patch order

| Review target | Applied changes | Purpose |
| --- | --- | --- |
| [CSMWrap](build.py) | [0001](patches/0001-am4-memory-serial-and-boot-policy.patch), [0003](patches/0003-preserve-coreboot-cmos-option-contract.patch), [0005](patches/0005-reclaim-temporary-csm-helper.patch) | Secure Boot refusal, memory and serial configuration, coreboot CMOS metadata transfer, temporary AP helper ownership |
| SeaBIOS inside CSMWrap | [0002](patches/0002-seabios-csm-serial.patch), [board CMOS policy](../../../src/mainboard/asrockrack/b565d4_v1l/seabios/0001-coreboot-cmos-payload-policy.patch), [0004](patches/0004-reproducible-seabios-version.patch), [0006](patches/0006-seabios-csm-thunks-resources-console.patch), [0007](patches/0007-preserve-firmware-console-UART.patch) | CSM serial configuration, coreboot CMOS options, reproducible version, real-mode thunk and FSEG budget, firmware-console UART ownership |
| MrChromebox EDK2 | [optional CSM application](../../../src/mainboard/asrockrack/b565d4_v1l/edk2/0003-optional-seabios-csm-application.patch), [CMOS boot policy](../../../src/mainboard/asrockrack/b565d4_v1l/edk2/0008-cmos-payload-boot-policy.patch) | Embed the EFI application in the firmware volume, register its boot entry, apply the selected boot preference |
| coreboot payload plumbing | [Kconfig](../edk2/Kconfig), [EDK2 Makefile](../edk2/Makefile), [external Makefile](../Makefile.mk) | Select and build the optional application before the EDK2 payload |

The order above reflects build.py: it applies 0001/0003/0005 to CSMWrap and
0002, the board CMOS policy, 0004/0006/0007 to its SeaBIOS submodule. The
numbered files are not one linear patch series against a single repository.

## Reusable mechanism and B565 policy

The wrapper and SeaBIOS mechanisms can be reviewed independently of B565:
the Compatibility16 application, same-CPU real-mode thunk, temporary helper
ABI, Secure Boot gate, CMOS metadata transfer, FSEG allocation, serial
ownership and reproducible version generation. The optional EDK2
firmware-volume entry and boot-menu registration are likewise payload
integration work.

The current build is still tied to this board in several places:

- EDK2_CSMWRAP depends on BOARD_ASROCKRACK_B565D4_V1L and the board's
  large-payload option in [Kconfig](../edk2/Kconfig).
- [build.py](build.py) sets LOW_STUB_BASE=0x40000, AST2500 UART2 at 0x2f8,
  the AM4 build version and the board SeaBIOS CMOS patch directly.
- The low-memory E820 reservation protects coreboot's B565 SIPI area.
- The [EDK2 CMOS boot policy](../../../src/mainboard/asrockrack/b565d4_v1l/edk2/0008-cmos-payload-boot-policy.patch)
  interprets this board's option layout and boot preference.
- Legacy boot depends on the board's EDK2 payload size, memory map, serial
  configuration and IOMMU handoff policy.

The review order is CSMWrap changes, SeaBIOS changes, EDK2
optional-application support, coreboot's build option, then B565 values and
boot policy. Making the first four layers portable requires turning the
currently hard-coded values and board patch path into explicit inputs and
testing a second compatible platform. Moving files alone would not establish
portability.

## Qualification boundary

The V26 B565 build and physical cold/warm tests are recorded in the separate
operator evidence archive. They show that the V26 UEFI boot path works on
that board; they do not exercise the CSM entry or test this patch stack on
another board. This review map changes documentation only.
The wrapper is an EFI application that invokes SeaBIOS CSM services. It does
not implement the complete historical EDK2 EFI_LEGACY_BIOS_PROTOCOL.
Protected/V86 BIOS calls need the temporary helper while firmware still owns
it; OS AP startup must not race those calls.

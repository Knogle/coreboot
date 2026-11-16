# B565D4-V1L runtime EFI variable storage

The experimental `B565D4_EDK2_NVSTORE` profile selects `VARIABLE_SUPPORT=FCH_SPI`.
It reuses EDK2's FVB, fault-tolerant-write and runtime-variable drivers, with
this board's AMD FCH storage library implementing the existing `SmmStoreLib`
interface. The interface name does not imply an SMI: this provider retains
`NO_SMM`, performs no SMI and exposes no whole-flash or capsule-write API.

Only SPI offsets `0x01037000..0x01056fff` are accessible. The existing upper
128-KiB NVAR volume holds 60 KiB of variables, a 4-KiB FTW working block and
64 KiB of spare space. The lower original NVAR volume is preserved. Before
initial formatting, the provider accepts the exact owned legacy volume
SHA-256 `4e18d5c2dafa732e932c177b97648ad41aae9a1a4a41d6e3688570bd2181f812`,
an erased allocation, or an existing valid system-NV-data volume. Unknown
contents are refused. The hash identifies the owned migration source; it is
not a firmware authentication mechanism.

The SPI100 implementation uses dedicated four-byte-address commands, finite
controller/flash deadlines, the AMD host-BIOS hardware mutex and a nonblocking
CPU lock. It preserves FIFO/opcode/count state and checks chip, controller and
ownership state. Programming requires only bit-clearing changes and verifies
each completed chunk. No address-mode, protection-unlock or reset command is
issued. An unknown write outcome disables this provider until processor reset.

The FVB exposes a page-aligned runtime RAM mirror. Verified writes and erases
update that mirror. The FCH MMIO page and RAM allocation are runtime resources;
their pointers are converted on the EFI virtual-address transition. The timer
uses the payload's ACPI timer library with rollover accounting. The HOST NOR
model checks bounds, page splits, erase, ownership races, failure outcomes and
deadlines. A successful model or payload build does not establish persistence
or Linux runtime-variable operation on the target.

After the first variable write, the upper NVAR volume is intentionally mutable.
Future external updates must preserve its actual contents using complete PRE
and POST captures and an explicit range-aware qualification. The previous
whole-image mirror assertion and old strict writers must not be reused blindly.
Signed PSP components and all other protected image ranges remain immutable.

Native310 evidence and physical qualification status are tracked in
`porting/asrock_am4/SETUP_OPTIONS_20261026.md` and the corresponding ignored
`Native310-BoardSetup-Root-controls-01` controls directory.

The provider initializes SerialPortLib from the bootloader UART HOB before
initialization traces. Standard payload PCI reads validate LPC790e1022 and
SPI BARfec10002 only during boot; runtime operations access only converted
FCH MMIO and the allocated store. Actual DEBUG comparison established that
ECAM and CF8 returned the same values; CF8 was not the cause of the initial
failure and its diagnostic library is removed from the next release build.

The actual GCD failure occurred before the first FCH operation. UefiPayload
sets PcdCpuDisableMtrrProgramming=TRUE, so requesting UC|RUNTIME causes CpuDxe
to reject a cache-attribute change. A runtime-only GCD operation preserves the
existing CPU attributes and adds the page to the runtime map. Actual all12
MTRR reads classify the FCH page UC; Linux runtime SetVariable/readback,
independent full SPI read and a following cold boot retained the owned nonce.
BootNext SATA consumption and Timeout7 persistence also passed. These are
DEBUG code-origin7512 observations; release build12 cleanup needs a new boot.

The standard upstream VariableRuntimeDxe does not reclaim a full variable
store at OS runtime. It returns EFI_OUT_OF_RESOURCES and ReclaimForOS runs
at EndOfDxe/ReadyToBoot. A bounded target test completed79 distinct384-byte
updates with exact readback before this limit, then deleted its owned variable.
Protected variables remained unchanged and all12 MCG_STATUS remained0. Do
not claim unlimited runtime updates,128-update success or runtime garbage
collection. Actual next-boot reclamation and renewed write capacity are pending.

The normal release candidate is composed offline from its independently
qualified pristine build and the fresh complete physical PRE. Only the actual
upper NVAR is overlaid. The selected writer still requires an exact new full
PRE, ordinary flashrom WRITE/VERIFY and independent complete POST, and checks
both original NVAR allocations literal to PRE. Whole-bank equality is claimed
only outside the explicit NVAR exception; no historical normal-WRITE receipt
is fabricated for an independently read mutable store.

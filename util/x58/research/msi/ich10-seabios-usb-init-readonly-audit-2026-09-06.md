> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# ICH10R to SeaBIOS USB initialization audit

Date: 2026-09-06

Status: read-only source, datasheet, and retained-capture audit.  No target was
contacted, no image was built, and no firmware or hardware register was
changed for this work.

## Result

The current selective MSI X58 Pro-E path is not missing a generic
"start the USB controller" operation in coreboot.  Its division of work is:

1. coreboot validates the eight ICH10R USB functions, assigns their BARs,
   enables only the BAR decode appropriate to each controller, leaves bus
   mastering off, and applies the documented ICH10 EHCIIR2 BIOS-required
   fields to both EHCIs;
2. coreboot logs, but does not alter, the relevant RCBA, ACPI-PM, GPIO,
   legacy-ownership, host-controller, and port state;
3. SeaBIOS enables bus mastering, resets each controller, builds DMA
   schedules, starts EHCI and UHCI, routes high-speed ports to EHCI, polls all
   root ports, and performs descriptor/configuration/HID setup if a port
   reports a connection.

The complete retained B06VP payload run proves that both EHCIs and all six
UHCIs were found and entered this SeaBIOS setup path.  They then released all
controller allocations without finding a device.  No descriptor read and no
`USB keyboard initialized` line occurred.  The subsequent i8042 `ff` timeout
is a separate PS/2 failure and does not explain USB.

The independent B06VP CAR censuses give the strongest register-level reason:

```text
all 12 EHCI port views: PORTSC = 00003030
all 12 UHCI port views: PORTSC = 0c80
```

Both forms mean current-connect-status clear and active-low overcurrent input
asserted; the EHCI values also contain the normal reset owner/power bits.
Intel documents that active EHCI overcurrent automatically disables a port.
The B06VP payload run and these CAR censuses are separate boots, so this is a
strongly correlated boundary, not a same-instruction-trace proof.  It is still
better evidence than any hypothesis that PIRQ, IOAPIC, ACPI, or a missing USB
interrupt route prevented initial discovery: SeaBIOS's initial enumeration is
polled, and it never reached the first descriptor transaction.

Consequently, the smallest justified automatic follow-up has an **empty new
USB write set**.  It should turn the existing pre-SeaBIOS telemetry into a
fail-closed, read-only admission/classification stage and retain the current
SeaBIOS USB trace.  No GPIO56-low, `5VDRV1_EN`, overcurrent-mux, PORTSC, PPO,
UPRWC, MAP, legacy-SMI, or clock-gating write is justified by the evidence.

## Scope and provenance limits

This audit compares:

- the retained B06VP SeaBIOS hardware run;
- four B06VP reset/port censuses and the later GPIO56-high-preserving
  experiment;
- Intel's public ICH10 Family Datasheet, document 319973-003;
- the selective board path represented by the stable B06VX config and
  devicetree;
- the current locally pinned SeaBIOS source and USB trace patch;
- the existing, excluded, generic coreboot ICH10 EHCI driver.

B06VQ and B06VQ-USBTRACE1 have no target-hardware run in the test matrix.  No
B06VX, B06VY, or B06VZ target USBTRACE capture exists in the repository at the
time of this audit.  Therefore:

- B06VP BAR values and controller state are observations of B06VP, not claimed
  as values observed from a later image;
- the later EHCIIR2 masked write is statically verified and manually motivated,
  but its effect on the target USB port state has not yet been captured;
- the QEMU USB trace validates instrumentation and the SeaBIOS software path,
  not the X58 Pro-E electrical USB path;
- source inspection occurred while another agent was extending post-B06VY
  resource code.  This note deliberately relies on the stable B06VX config and
  does not fingerprint or modify that shared implementation file.

The board-level VBUS/overcurrent investigation is kept in
[`ich10-usb-overcurrent-vbus-audit-2026-09-06.md`](ich10-usb-overcurrent-vbus-audit-2026-09-06.md)
and
[`usb-power-gpio56-high-followup-2026-09-06.md`](usb-power-gpio56-high-followup-2026-09-06.md).
This note does not assign an unproved meaning or polarity to `5VDRV1_EN` or
GPIO56.

## USB function topology and ownership

The selective PCI allowlist expects exactly the normal-header ICH10R functions
below.  Coreboot validates the exact device ID and USB class before normal PCI
resource programming; SeaBIOS subsequently dispatches by programming
interface (`0c0300` for UHCI and `0c0320` for EHCI).

| BDF | Function | Expected PCI ID |
|---|---|---|
| `00:1a.0` | UHCI4 | `8086:3a37` |
| `00:1a.1` | UHCI5 | `8086:3a38` |
| `00:1a.2` | UHCI6 | `8086:3a39` |
| `00:1a.7` | EHCI2 | `8086:3a3c` |
| `00:1d.0` | UHCI1 | `8086:3a34` |
| `00:1d.1` | UHCI2 | `8086:3a35` |
| `00:1d.2` | UHCI3 | `8086:3a36` |
| `00:1d.7` | EHCI1 | `8086:3a3a` |

The B06VX devicetree binds all eight functions to the selective
`b06vn_endpoint_ops`.  Those operations use generic PCI resource discovery,
assignment and decode enable, plus the board's strict identity/quiescence
gate.  They have no device `.init` callback.  In particular, they do not call
the excluded full ICH10 EHCI driver's `.init` callback.

That omission is not the observed fault.  The generic driver's entire EHCI
`.init` action is to OR `PCI_COMMAND_MASTER`; SeaBIOS does that immediately
before it programs DMA schedules.  Enabling the complete historical ICH10
southbridge path would also bring broad, unaudited board policy, function
hiding/locks, and clock-gating writes.  It is not a safe shortcut to USB.

## What coreboot already writes

### PCI resource and command state

Before enumeration, the selective preflight clears `PCI_COMMAND.IO`,
`PCI_COMMAND.MEMORY`, and `PCI_COMMAND.MASTER` after validating every allowed
root function.  Generic resource sizing and assignment then produces:

- a 32-byte I/O BAR4 for each UHCI and `PCI_COMMAND.IO=1`;
- a 1-KiB MMIO BAR0 for each EHCI and `PCI_COMMAND.MEMORY=1`;
- bus mastering still clear for every USB controller at payload handoff.

The full B06VP run observed the following allocation.  These are useful
reference values, not fixed-address requirements for later allocators:

| Controller | B06VP decoded base | B06VP command |
|---|---:|---:|
| `00:1a.0` | BAR4 base `dfe0` | `0001` |
| `00:1a.1` | BAR4 base `dfc0` | `0001` |
| `00:1a.2` | BAR4 base `dfa0` | `0001` |
| `00:1d.0` | BAR4 base `df80` | `0001` |
| `00:1d.1` | BAR4 base `df60` | `0001` |
| `00:1d.2` | BAR4 base `df40` | `0001` |
| `00:1a.7` | BAR0 base `cfcff000` | `0002` |
| `00:1d.7` | BAR0 base `cfcfe000` | `0002` |

The generic PCI resource code may also set conventional cache-line size,
latency, and IRQ-line fields.  These are ordinary PCI enumeration effects;
they are not ICH10 USB electrical initialization.

### EHCIIR2, PCI configuration offset `0xfc`

The only USB-specific coreboot mutation in the selective stage is a masked
read/modify/write of EHCIIR2 on both `00:1a.7` and `00:1d.7`:

```text
target = (before & ~0x2002000c) | 0x20020008
```

Intel ICH10 EDS section 17.1.36 requires BIOS to set bit 29, bit 17, and bits
3:2 to `10b`.  The helper preserves every other field.  The caller saves both
full pre-values, logs the derived targets, performs the helper once, and
requires exact full-dword readback.  From the B06VP reset value
`0x20001706`, this formula yields `0x2002170a`.  The vendor's observed
`0x2002130a` differs in preserved fields and is not a literal to copy.

The associated stage uses POST `44` for begin, `46` for exact success, and
terminal `47` for a duplicate attempt or readback failure.

### RCBA FD baseline

Later selective platform stages also write documented non-USB fields in RCBA
FD, including the BIOS-required bit 0 and, after the AHCI route stages, the
separate SATA2-disable field.  The USB-disable mask is
`0x0000bf80`:

| FD bit | Function disabled when one |
|---:|---|
| 15 | EHCI1 |
| 13 | EHCI2 |
| 12 | UHCI5 |
| 11 | UHCI4 |
| 10 | UHCI3 |
| 9 | UHCI2 |
| 8 | UHCI1 |
| 7 | UHCI6 |

The selective target keeps that mask zero.  Admission must compare the mask,
not a B06VP-era full FD literal, because later SATA policy legitimately changes
unrelated FD bits.

## What coreboot currently observes but does not write

The inherited pre-SeaBIOS routine logs each item below after PCI resources are
assigned.  Invalid BAR/decode/D0/capability-header conditions currently print
an error and return from the individual logger; they are not yet one atomic
USB admission decision.

### Chipset-global controls

| Register | Current use | Audit conclusion |
|---|---|---|
| RCBA `0x3418` FD | log USB-disable mask | Require mask `0`; do not overwrite full FD. |
| RCBA `0x341c` CG | log bit 20 | Bit 20 clear means EHCI is not statically clock-gated.  Bits 19 and 29:28 are optional dynamic clock-gating policy; zero disables power saving rather than disabling controller function.  No write is needed for discovery. |
| RCBA `0x3524` PPO | log low 12 bits | Zero means no USB port is electrically disconnected by PPO.  Do not write it. |
| RCBA `0x35f0` MAP | log bit 0 | Zero selects UHCI6 at D26:F2 and matches the exposed topology.  MAP is write-once/lock-sensitive policy in the full driver; do not self-write it merely to lock it. |
| PMBASE `+0x3c` UPRWC | log word | It gates writes to per-port chipset configuration.  A zero read is not a VBUS-off indication.  No per-port chipset write is proposed, so leave it unchanged. |
| GPIO use-select banks | log all OC mux bits | Every relevant bit is zero/native in B06VP.  Do not convert OC pins to GPIO. |

The current code does not write RCBA CG, PPO or MAP, PMBASE UPRWC, or any OC
GPIO mux.  This is deliberate and should remain so.

### EHCI PCI configuration

| Offset | EDS role | Current selective action | Proposed disposition |
|---:|---|---|---|
| `0x04` | PCI command | MEM decode assigned/enabled, BME clear | Gate exact relevant bits; SeaBIOS owns BME. |
| `0x10` | BAR0 | allocated 1-KiB MMIO BAR | Gate alignment/aperture; no fixed literal. |
| `0x54` | PMCSR | read | Require D0; do not write power policy. |
| `0x61` | FL_ADJ/config byte | read | Telemetry only; no missing start bit established. |
| `0x68` | legacy extended capability | read | Record BIOS/OS ownership.  Do not claim or clear ownership without a current nonzero target capture. |
| `0x6c` | legacy control/status | read/masked log | Record SMI enable/status bits; do not blind-clear W1C/status fields. |
| `0x70` | special SMI control/status | read/masked log | Same: classify, do not mutate. |
| `0x84` | EHCIIR1 | read | Telemetry only; no evidence-backed new write. |
| `0xfc` | EHCIIR2 | documented masked RMW | Keep current exact derived-target write/readback. |

SeaBIOS itself contains the explicit comment `check for and disable SMM
control?`; it does not currently take ownership by writing `0x68`, `0x6c`, or
`0x70`.  Existing research reports captured ownership/SMI enables clear, but
the B06VP full payload log does not preserve raw exact values.  A future gate
must first capture exact current-image values.  It must not invent a universal
zero literal or perform mixed W1C read/modify/write.

### UHCI PCI configuration

| Offset | EDS role | Coreboot action | SeaBIOS action |
|---:|---|---|---|
| `0x04` | PCI command | IO decode assigned/enabled, BME clear | enables IO and BME |
| `0x20` | BAR4 | allocates 32-byte I/O range | consumes it |
| `0xc0` | USB_LEGKEY | read/log only | writes `USBLEGSUP_RWC` (`0x8f00`) in `reset_uhci()` to reset legacy PIRQ/SMI status/control |
| `0xc8` | chipset UHCI configuration | read/log only | unchanged |
| `0xca` | chipset UHCI configuration | read/log only | unchanged |

ICH10 EDS section 16.1.23 gives USB_LEGKEY reset value `0x2000` and defines a
mixture of PIRQ enable, SMI/trap enable, and W1C status bits.  The vendor
runtime value `0x2f00` is therefore mixed status/policy and is not a value to
copy into coreboot.  SeaBIOS already owns its controller-reset transition.

Wake/resume policy, including the separate USB resume register, is outside
initial enumeration and is not a prerequisite for a cold boot keyboard.

## What SeaBIOS actually does

The exact configured payload source is commit
`5497f43189374647b3b0f282aa497e71c891f3c5`.  Its X58 config has UHCI, EHCI,
USB hub, mass storage, and USB keyboard support enabled; hardware IRQs and
debug level 9 are enabled; controller threads are disabled for deterministic
serial ordering.  The image supplies `etc/usb-time-sigatt=1000` ms.

With threads off, `device_hardware_setup()` calls `usb_setup()` before
`ps2port_setup()` and storage.  `usb_setup()` processes EHCI before UHCI.

### EHCI sequence

For every PCI function with EHCI programming interface, SeaBIOS:

1. validates/enables BAR0 memory decode;
2. reads CAPLENGTH, HCSPARAMS and HCCPARAMS and locates operational registers;
3. enables PCI bus mastering;
4. allocates one periodic frame list and the interrupt/async queue heads;
5. records pre-reset USBCMD/USBSTS;
6. writes HCRESET while disabling schedule enables and polls for reset clear
   with a 250-ms timeout;
7. writes USBINTR zero;
8. programs PERIODICLISTBASE and ASYNCLISTADDR;
9. writes USBCMD RUN, asynchronous-schedule enable and periodic-schedule
   enable;
10. writes CONFIGFLAG one, routing ports to EHCI;
11. waits 20 ms, records every PORTSC, and polls each port for a connection;
12. resets a connected high-speed device or transfers low/full-speed ownership
   to its UHCI companion;
13. if no device was found, stops the controller and releases its schedules.

The ICH10 EHCI `PORT_POWER` bit reads one as a hardwired chipset property.  A
SeaBIOS attempt to set it when clear is not proof of physical 5-V VBUS and
cannot repair an asserted external OC input.

### UHCI sequence

For every PCI function with UHCI programming interface, SeaBIOS:

1. validates/enables BAR4 I/O decode and enables bus mastering;
2. writes USB_LEGKEY `0x8f00` as part of `reset_uhci()`;
3. writes global host-controller reset, waits 5 microseconds, then writes
   USBINTR and USBCMD zero;
4. builds and programs its 1-KiB frame list and queue heads;
5. writes SOF `0x40`, frame-list base, and frame number zero;
6. writes USBCMD Run/Stop, Configure Flag and Max Packet bits;
7. waits for EHCI setup ordering, records both ports, and polls CCS;
8. resets and enumerates a connected low/full-speed device;
9. if no device was found, stops and releases the controller.

Initial enumeration and all control transfers use polling and bounded timer
loops.  USB controller PIRQ delivery, IOAPIC routing, SCI routing, and ACPI are
therefore not prerequisites for reaching GET_DESCRIPTOR or printing
`USB keyboard initialized`.

After a keyboard has been configured, SeaBIOS places its interrupt pipe on
the controller schedule.  The legacy keyboard interface is serviced when the
timer path calls `usb_check_event()`, then `usb_check_key()`, then
`usb_poll_intr()`.  Thus a working periodic timer is required for subsequent
keystrokes.  B06VP progressed through DHCP, TFTP and the netboot menu, but
because no USB keyboard pipe was ever created, that run is not a test of this
later polling path.

## What B06VP proves

The full retained capture
[`2026-09-06-b06vp-hw-03-full-to-netboot-menu.raw`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
contains:

- 68 enumerated PCI functions;
- `init usb`;
- EHCI setup at `00:1a.7`, BAR0 `cfcff000`, operational base
  `cfcff020`;
- EHCI setup at `00:1d.7`, BAR0 `cfcfe000`, operational base
  `cfcfe020`;
- schedule allocation and then complete freeing for both EHCIs;
- UHCI setup at all six functions, using I/O bases `dfe0`, `dfc0`, `dfa0`,
  `df80`, `df60`, and `df40`;
- schedule allocation and then complete freeing for every UHCI;
- no device descriptor/configuration/HID setup and no
  `USB keyboard initialized`;
- only afterwards, `init ps2port`, repeated i8042 status `ff`, and a bounded
  i8042 timeout;
- later successful Radeon VBIOS/display, RTL8168 link, DHCP, TFTP, iPXE and
  netboot.xyz menu progress.

This proves PCI discovery, valid BAR decode, controller object allocation,
and entry into both EHCI and UHCI setup.  The paired allocation/free pattern
is the SeaBIOS no-device path.  It does not prove a physical keyboard was
powered or connected, nor that a descriptor request failed: no such request
was attempted.

The separate read-only/reset censuses additionally establish on B06VP:

```text
EHCI identity:       exact expected IDs, D0
EHCI CAPLENGTH:      20
EHCI HCIVERSION:     0100
EHCI HCSPARAMS:      00103206 (six ports per controller)
EHCI HCCPARAMS:      00016871
EHCI USBCMD:         00080000 before reset census
EHCI USBSTS:         00001004
EHCI USBINTR:        00000000
EHCI CONFIGFLAG:     00000000
EHCI all PORTSC:     00003030

UHCI USBCMD:         0000 after reset census
UHCI USBSTS:         0020
UHCI all PORTSC:     0c80
```

Every temporary BAR/decode mutation in those ROMMON scripts rolled back
exactly, and no host-controller operational register was written.  A later
test changed GPIO56 direction from input to output while preserving its
already-high level; every EHCI2 PORTSC remained `00003030`, and rollback also
succeeded.  This rejects only that narrow high-preserving direction
hypothesis.  It does not authorize GPIO56-low or any `5VDRV1_EN` write.

## Newly measured PCI interrupt pins do not conflict

The later read-only USB routing census measured:

| Function group | PCI interrupt-pin bytes |
|---|---|
| D26: F0/F1/F2/F7 | INTA/INTB/INTC/INTC (`A/B/C/C`) |
| D29: F0/F1/F2/F7 | INTA/INTB/INTC/INTA (`A/B/C/A`) |

These are PCI INTx pin declarations, not USB overcurrent pins.  They do not
conflict with the no-CCS/active-OCA diagnosis.  The same census found the
current route words `D26IR=3210` and `D29IR=3210`, whereas MSI's vendor route
words are `3250` and `0237`.  Combining the measured pin bytes with the vendor
route words derives GSIs 16/21/18/18 for D26 and 23/19/18/23 for D29.

That difference matters for a future OS or firmware INTx-routing milestone,
but it cannot explain the current SeaBIOS enumeration boundary:

- SeaBIOS writes EHCI and UHCI USBINTR zero and polls root-port/connect and
  transfer completion state;
- no USB device reached a state where an interrupt pipe existed;
- after a keyboard is configured, SeaBIOS polls the controller schedule from
  its PIT timer handler rather than depending on a host-controller INTx.

Therefore no D26IR/D29IR write belongs in the proposed USB admission stage.
The immutable census raw file has SHA-256
`b5d63502afbdfaf013d8ca6abac33691e65a4d6701300a8ce57cefd13756e7b0`;
its metadata has SHA-256
`02a984d5fc64b469b82fa811ca46644d7263e7bc836ff457f56eccbe2acfdce1`.

## Why keyboard input is not yet proved

The failure hierarchy is now:

1. **Proved:** all eight USB PCI functions enumerate and SeaBIOS starts both
   controller families.
2. **Proved on separate B06VP CAR boots:** no port reports CCS and every port
   reports active overcurrent.
3. **Proved:** SeaBIOS therefore has no device on which to issue the first
   descriptor transfer in the retained full run.
4. **Not yet tested on target:** the USBTRACE-instrumented B06VX-or-later path.
5. **Not yet reached on target:** successful HID boot-protocol setup and
   interrupt-pipe creation.
6. **Not yet proved:** an actual keystroke entering the SeaBIOS boot menu.

The leading remaining boundary is the board's VBUS/OC discrete chain, not a
known missing PCI-configuration or RCBA write.  The following do not fit the
evidence as the first failure:

- PIRQ/IOAPIC/SCI/ACPI routing: initial enumeration is polled and CCS is
  already zero;
- EHCI bus mastering: SeaBIOS enables it and successfully constructs/frees
  schedules;
- EHCI CONFIGFLAG: SeaBIOS sets it before scanning;
- UHCI legacy cleanup: SeaBIOS performs it before UHCI scanning;
- RCBA FD/PPO/MAP or static EHCI clock gate: B06VP/current policy exposes the
  functions, does not disconnect the ports, uses the matching UHCI6 map, and
  leaves the static gate clear;
- physical port-power status bit: ICH10 EHCI hardwires it high, so it cannot
  attest VBUS.

## Smallest fail-closed automatic follow-up

### New mutation set

```text
none
```

The stage should inherit the existing, separately reviewed writes:

- PCI BAR assignment and correct IO/MEM decode;
- bus mastering clear at coreboot handoff;
- EHCIIR2 masked BIOS-required-field RMW with exact derived readback;
- non-USB ICH baseline/AHCI/IOAPIC/resource stages already selected by its
  predecessor.

It should add only one atomic pre-payload USB admission/classification pass.
This is preferable to duplicating SeaBIOS reset/schedule work or changing
registers whose observed state already permits controller execution.

### Structural gates

Before reading controller operational registers, require:

1. exact LPC ID `8086:3a16`, enabled fixed RCBA, and the existing exact PMBASE
   and GPIOBASE decode gates;
2. all eight exact USB PCI identities, normal header type, and programming
   interface (`0c0300`/`0c0320`);
3. PMCSR D0 for both EHCIs;
4. each UHCI BAR4 is a 32-byte-aligned I/O BAR wholly inside the allocated PCI
   I/O aperture, with IO decode on and MEM/BME off;
5. each EHCI BAR0 is a 1-KiB-aligned MMIO BAR wholly inside the allocated PCI
   MMIO aperture, with MEM decode on and IO/BME off;
6. EHCIIR2 equals the target derived from that controller's saved pre-value,
   never a vendor full-dword literal;
7. FD USB-disable mask equals zero, CG bit 20 equals zero, PPO low 12 bits
   equal zero, MAP bit 0 equals zero, and every USB OC pin remains selected for
   its native function;
8. both EHCI capability tuples are exactly CAPLENGTH `20`, HCIVERSION `0100`,
   six ports, and the target-observed HCCPARAMS `00016871`.

UPRWC should be printed but not treated as a VBUS boolean.  CG dynamic-gating
fields should be printed but not required to match vendor power-saving policy.
Never gate on the complete FD or CG dword when unrelated inherited stages are
allowed to change other fields.

### Ownership and quiescence gates

For each EHCI, log exact full `0x68`, `0x6c`, and `0x70` values and decode the
BIOS-owned/OS-owned and SMI-enable bits.  Until a current automatic-image
capture proves their exact prestate, any active BIOS ownership or SMI enable
should be classified `UNSAFE_LEGACY_OWNER` and must not be cleared
automatically.  Treat that class as terminal until it receives its own
one-field review.  This is fail-closed with respect to a new write while
retaining the raw evidence.

The B06VP CAR quiescent tuple supports checking that, before SeaBIOS:

- EHCI USBCMD has RUN, HCRESET, ASE and PSE clear;
- EHCI USBINTR is zero;
- EHCI CONFIGFLAG is zero;
- UHCI USBCMD Run/Stop and reset bits are clear;
- UHCI USBINTR is zero.

Do not compare entire status registers to literals: USBSTS contains sticky and
implementation-status fields.  Do not clear status as part of admission.

### Port classifier

Read all six ports from each EHCI and two ports from each UHCI.  Log complete
raw values plus controller-local and aggregate CCS/OCA bitmaps.  The outcome
classes should be:

| Observation | Class | Action |
|---|---|---|
| OCA clear, CCS set on any view | `READY` | Continue to SeaBIOS USBTRACE. |
| OCA clear, CCS clear everywhere | `NO_DEVICE` | Non-fatal; continue so an unattached-device boot remains valid. |
| OCA set with CCS clear | `EXTERNAL_BLOCKED` | Non-fatal diagnostic; perform no port/GPIO/power write and continue to SeaBIOS for corroborating trace. |
| incoherent port count, impossible capability tuple, invalid BAR/decode/D0, USB function hidden, or contradictory OC state for a datasheet-defined EHCI/UHCI companion pair | `FAIL` | Terminal fail before payload; no USB operational mutation. |

OCA itself must not be a terminal condition.  The current purpose is to retain
the SeaBIOS trace and distinguish external electrical blockage from a software
controller failure.  Similarly, `NO_DEVICE` is a valid boot state.

The classifier must not acknowledge EHCI OCC or UHCI OCI.  Those are mixed
PORTSC W1C registers; clearing the sticky history cannot clear the live OCA
input and would destroy evidence.

### POST and serial contract

Numeric POST values must be assigned only after auditing the then-current
stage map; B06VY already owns `70` through `74`.  Suggested symbolic events
are:

```text
POST_USB_ADMIT_BEGIN
POST_USB_ADMIT_STRUCT_OK
POST_USB_ADMIT_EXTERNAL_BLOCKED
POST_USB_ADMIT_READY
POST_USB_ADMIT_FAIL
```

One contiguous serial block should contain:

```text
[USB-ADMIT] BEGIN BUILD=<exact-id>
[USB-ADMIT] CHIPSET LPC=... RCBA=... FD=... USB_DIS=... CG=... PPO=... MAP=... UPRWC=...
[USB-ADMIT] EHCI BDF=... ID=... CLASSPI=... CMD=... BAR=... PMCSR=... EHCIIR2=...
[USB-ADMIT] EHCI BDF=... CAPLEN=... HCIVER=... HCSPARAMS=... HCCPARAMS=...
[USB-ADMIT] EHCI BDF=... LEG68=... SMI6C=... EXT70=... CMDSTSINTRFLAG=...
[USB-ADMIT] EHCI BDF=... PORTS=<six raw dwords> CCS=<bitmap> OCA=<bitmap>
[USB-ADMIT] UHCI BDF=... ID=... CLASSPI=... CMD=... BAR4=... LEGKEY=... CFGC8=... CFGCA=...
[USB-ADMIT] UHCI BDF=... CMDSTSINTR=... PORTS=<two raw words> CCS=<bitmap> OCA=<bitmap>
[USB-ADMIT] RESULT=<READY|NO_DEVICE|EXTERNAL_BLOCKED|FAIL> WRITES=0
[USB-ADMIT] END
```

All reads must be bounded by already-validated decode and capability length.
A duplicate attempt or a changing identity/BAR during the block is a terminal
failure.  The block must explicitly say `WRITES=0` so a later capture can
distinguish observation from attempted recovery.

### SeaBIOS success contract

The subsequent deferred journal is valid only if it has one complete
`[USBTRACE] BEGIN count=N dropped=0` through `[USBTRACE] END` block with
contiguous indices.  A keyboard success requires, for one consistent
controller/port chain:

1. controller reset post-event with success;
2. controller RUN event;
3. port event showing CCS;
4. device detect and port reset with a non-negative speed;
5. SET_ADDRESS success;
6. first eight descriptor bytes;
7. configuration header and full configuration;
8. SET_CONFIGURATION success;
9. HID boot-protocol and interrupt-pipe setup success;
10. literal `USB keyboard initialized`;
11. a deliberately tested key changing the SeaBIOS boot-menu selection.

Enumeration through `USB keyboard initialized` proves the controller/device
path.  Only the final tested keystroke proves runtime input and the later timer
poll path.  The existing QEMU validation demonstrates that the journal can
represent all earlier steps with 24 contiguous entries and zero drops; it does
not satisfy the target-hardware criterion.

## Explicitly rejected writes

The next stage must not add:

- GPIO56 direction or level changes, especially GPIO56-low;
- any inferred `5VDRV1_EN`, socket-occupancy, power-good, or rail-control
  write;
- GPIO use-select changes for OC0# through OC11#;
- PPO or UPRWC writes;
- MAP self-write/lock;
- EHCI or UHCI PORTSC writes, including W1C acknowledgements;
- EHCI legacy ownership/SMI clears at `0x68`, `0x6c`, or `0x70`;
- vendor USB_LEGKEY literal `0x2f00`;
- CG static or dynamic clock-gating changes;
- duplicate BME, HCRESET, schedule-base, RUN, CONFIGFLAG, or port-reset work in
  coreboot;
- broad ICH10 driver enablement;
- PIRQ/SCI/ACPI changes under the label of USB discovery.

If a later run removes OCA and reaches CCS but USBTRACE stops after a specific
event, that last successful event may justify a new one-field hypothesis.
Until then, none of these writes addresses the measured boundary.

## Failure and recovery behavior

The proposed admission stage has no new persistent or operational USB write to
roll back.

- Structural or identity failure: print the exact mismatching register and
  expected mask/value, emit terminal `POST_USB_ADMIT_FAIL`, and do not enter
  SeaBIOS.
- External OCA classification: print all bitmaps, emit
  `POST_USB_ADMIT_EXTERNAL_BLOCKED`, make no recovery write, then enter SeaBIOS
  so USBTRACE supplies an independent controller view.
- No attached device: print `NO_DEVICE` and continue normally.
- Ready port: print `READY` and continue normally.
- SeaBIOS EHCI HCRESET failure: its existing poll is bounded to 250 ms and the
  USBTRACE reset-post result identifies the boundary.
- Payload failure after inherited platform mutations: use the established
  socketed-chip recovery image and one-second AC interruption procedure; this
  audit does not change that procedure.

The first useful hardware run should use one known USB-2.0 boot keyboard
directly on a rear ICH10 port, with no hub or extension, COM1 capture armed
before power-on, and a recorded cold/warm-reset provenance.  It should retain
both the complete `[USB-ADMIT]` block and complete `[USBTRACE]` block even if
video appears.

## Reproducible inputs and identities

| Input | SHA-256 |
|---|---|
| Intel ICH10 EDS local research PDF `/tmp/ich10-319973-003.pdf` | `b7436502827a9ff0cb8c6921682764b33a825d9618dbadf88d28680936dff96b` |
| B06VP full SeaBIOS/netboot capture | `6659101eb3abd58bdca89f90d5dc2a8ed8824d9b07ebcccdcb4a86888d779e18` |
| B06VP EHCI1 reset/port census | `477d77e31f5b30795768334f753cc1161c5087cd7727b63051ff7b2016a0f09d` |
| B06VP EHCI2 reset/port census | `db132f36f69f6a7733c017fdc956d49dcc9990433c25233c5b029c0ab84761f6` |
| B06VP D26 UHCI reset/port census | `5ceed79262b628db2aab3c3b0ad12328bfff9b01b8d614b7c9a595b9dd58727b` |
| B06VP D29 UHCI reset/port census | `96fdc6ebc48878f35b27779ed000278f6ae8ff1ec7b4181683d5b1570240a628` |
| B06VP GPIO56-high-preserving EHCI2 run | `f9dc9628e60c4c7e0d17e7a1dfd3cf3ba50074419949951971c549998f91c99c` |
| B06VP USB INTx-pin/routing census | `b5d63502afbdfaf013d8ca6abac33691e65a4d6701300a8ce57cefd13756e7b0` |
| B06VP USB INTx-pin/routing metadata | `02a984d5fc64b469b82fa811ca46644d7263e7bc836ff457f56eccbe2acfdce1` |
| selective ICH10 `ehci_init.c` | `38b3ab4757327fbdddbad96d459dc6c98676cc6954f3ab4644b5312deb5e4f8b` |
| excluded generic ICH10 `usb_ehci.c` | `fcdbee073054cbb21fc0ba48071ba6e492d8077e979a278582b605b715f0dd52` |
| SeaBIOS `src/hw/usb.c` | `2044ef2e45a370f6f31744070c17650ae5b2ec8e4958fbc090dfa88288a33ccd` |
| SeaBIOS `src/hw/usb-ehci.c` | `36c80c57f3a9d458318d2261f714f4bb9716c8beeb3ce82c35b26d5a5feed3d0` |
| SeaBIOS `src/hw/usb-uhci.c` | `f2e175a9c866a96df2dd882affeeffa87a3c707e353529ed1520d6458a90a323` |
| SeaBIOS `src/hw/usb-hid.c` | `4ac5f84b8779e4678432d1c26142fbef7157545a905966d1b0e334b0e9bce01c` |
| SeaBIOS X58 USBTRACE config | `732694440794916ab81333bedd8645f075bd6289da941d4034ed750e8abf9f0f` |
| stable B06VX coreboot config | `1838cce8223122edcd109b53e2294927273545fc9758114e4bf82dde791248c5` |
| stable B06VX devicetree | `67f9439fc4809b3c481c5b478e1b15dfdb9ed5a6f8b3f1a3f4f58553514ac07c` |
| QEMU USBTRACE validation note | `35f5bc152f75945bf768bd497bf1ef841c54b28ff25ed3f26793f705e53851e1` |
| frozen SeaBIOS USBTRACE patch, as recorded by the validation note | `f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f` |

Relevant ICH10 EDS sections are 10.1.77 (FD), 10.1.78 (CG), 10.1.82
(PPO), 10.1.84 (MAP), 13.8.3.15 (UPRWC), 16.1.23 (USB_LEGKEY), 16.2
(UHCI operational registers), 17.1.26 through 17.1.28 (EHCI legacy/SMI),
17.1.36 (EHCIIR2), and 17.2 (EHCI capability, operational, and PORTSC
registers).

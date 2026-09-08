> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06J CAR ROMMON and SerialICE endpoint

B06J is an experimental interactive monitor which runs entirely in the
existing 64-KiB cache-as-RAM window.  It enters after the already demonstrated
CPU-signature, ICH10R BAR, SMBus-controller, Fintek, and COM1 setup.  It does
not initialize QPI, the IMC, DDR3, X58, normal DRAM, ramstage, or a payload.

The physical interface is COM1/JCOM1 at `0x3f8`, 115200 baud, 8 data bits, no
parity, one stop bit, and no flow control.  B06H proved transmission on this
path; B06J is the first image intended to test receive.

## Safe first test

After a full AC-off start, successful entry ends at POST `0xbc` and prints:

```text
B06J ROMMON in CAR; no DRAM. Type help.
rommon>
```

Start with read-only commands:

```text
id
help
pci 0 1f 0 0 l
pci 0 1f 3 0 l
spd
```

The two PCI reads should normally return `3a168086` and `3a308086`.  `spd`
runs the B06I fixed-`0x54` experiment on demand: base bytes `0x00..0x7f` once,
bytes `0x80..0xff` twice, equality check, fingerprints, the existing coreboot
DDR3 decoder, and a non-programmed DDR3-800 cycle candidate.  A successful
command reports result `b7` and then returns to the monitor at `bc`.  If an
I801 transaction times out at `50`, B06J latches the condition and refuses
another `spd` command until complete AC removal.

## Human command interface

All numbers are hexadecimal.  Width is byte (`b`), word (`w`), or dword (`l`)
and defaults to dword.

```text
help
id
cpuid LEAF [SUB]
msr INDEX
io PORT [b|w|l]
pci BUS DEV FN REG [b|w|l]
mem ADDR [b|w|l]
dump ADDR COUNT
spd
lock
unlock WRITE
msrw INDEX HI LO
iow PORT VALUE [b|w|l]
pciw BUS DEV FN REG VALUE [b|w|l]
memw ADDR VALUE [b|w|l]
post VALUE
halt
serialice
```

`dump` is limited to `0x40` bytes.  Human write commands are locked by
default.  The exact uppercase phrase `unlock WRITE` arms precisely one write;
the arm is consumed before the access and the prompt changes to `rommon[W]>`.
`lock` cancels it.

This gate prevents an accidental write caused by a mistyped human command; it
cannot make an arbitrary access safe.  An invalid or side-effecting I/O,
PCI-config, MMIO, or MSR address can still hang, reset, or damage platform
state.  In particular, do not write IMC/QPI/clock/voltage/reset registers
without a separately documented hypothesis and recovery plan.

## SerialICE-compatible stream

The attached research suggestion is incorporated as a second, deliberately
separate mode.  Enter it with the line command `serialice`, or send `*` as the
first byte at an empty ROMMON prompt to start a manual stream command.  An `@`
as the first byte performs the standard SerialICE-QEMU prompt handshake and
enters stream mode automatically.  Stream mode is permanent until reset and
its writes execute immediately; the human one-shot write gate does not apply.

B06J implements the documented SerialICE v1.5 byte stream:

```text
*rmAAAAAAAA.b|w|l
*wmAAAAAAAA.b|w|l=VALUE
*riPPPP.b|w|l
*wiPPPP.b|w|l=VALUE
*rcIIIIIIII.KKKKKKKK
*wcIIIIIIII.KKKKKKKK=HHHHHHHH.LLLLLLLL
*ciEEEEEEEE.CCCCCCCC
*mb
*vi
```

Input is echoed, commands do not need Enter, and each transaction returns to
the `> ` stream prompt.  `q`/64-bit memory accesses are intentionally absent.
`*mb` returns the required 32-character padded board field, and `*vi` retains
the protocol's LF-only version framing.  These details and the `@` handshake
were checked against the QEMU-side parser, not inferred from the examples.
PCI configuration-space accesses from an emulator are conveyed through its
normal CF8/CFC I/O operations, so no extra PCI command is needed in the
SerialICE protocol.  The historical MSR key field is accepted for wire
compatibility; Intel `RDMSR`/`WRMSR` do not consume it.

The implementation follows the primary SerialICE command documentation and
shell source:

- [SerialICE shell commands](https://github.com/coreboot/serialice/blob/main/SerialICE/README.SHELL)
- [SerialICE QEMU forwarding model](https://github.com/coreboot/serialice/blob/main/SerialICE/README.QEMU)
- [SerialICE source repository](https://github.com/coreboot/serialice)
- [SerialICE QEMU repository](https://github.com/coreboot/serialice-qemu)

Protocol compatibility has been checked against the source and generated
machine code.  A real UART-RX session and a SerialICE-QEMU session have not yet
been executed, so B06J must not be described as runtime-proven.

## POST codes

```text
bc  ROMMON ready / human command completed
bd  human command accepted for dispatch
be  UART RX/TX failure in monitor operation
bf  parse, argument, lock, or SerialICE command error
c7  SerialICE stream entered; later forwarded port-80 writes may replace it
```

The inherited B04/B06 hardware errors retain their existing meanings.  During
the manual `spd` command, B06I progress/result codes `a0..bb` and inherited
SMBus errors are visible.  No normal OS boot is expected from B06J.

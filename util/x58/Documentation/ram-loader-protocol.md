> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# X58 ROMMON RAM-loader protocol v1

## Scope and status

This protocol is an experimental post-memory-init debug facility. It is not a
boot protocol, a firmware-update protocol, or proof that the loaded object is
safe to execute. The implementation is deliberately split into:

1. a transport-independent, streaming object sink;
2. a fixed framed UART adapter;
3. a separately armed raw-code execution gate.

The sink can later receive the same object from a minimal TFTP client without
changing its validation, CRC, range, or execution rules. B06V6 integrates the
sink with the `ramload` command and a binary-safe COM1 adapter. It also has a
read-only `netprobe` PCI snapshot and a passive `timer` probe, but no NIC
configuration write, PHY reset, DMA, Ethernet, IP, UDP, TFTP, SSH, or payload
call. In particular, an SSH server is not a reasonable first post-MINIT
target: it requires a working NIC and DMA path, timers, interrupts or polling,
an IP/TCP implementation, cryptography, authentication state, and
substantially more trusted code. A small polling
Ethernet/ARP/IPv4/UDP/TFTP path is the appropriate network step after PCI
resources, the onboard NIC, DMA, and a stable RAM window are proven. It will
be a ROMMON file-transfer command, not a network boot option.

The B06V6 UART integration drains a bounded CR/LF command tail before emitting
`READY`. The host must wait for a valid `XRA1 READY` frame before transmitting
the first `XRL1` header byte; the supplied sender does so.

B06V6 reserves `0x02000000..0x027fffff` as an 8-MiB debug workspace outside
CBMEM at `0x01000000..0x017fffff`; the accepted size of one object is at most
4 MiB and read-back verification is mandatory. The address is supplied as
integration policy and is not compiled into the transport-neutral core.

The same `0x02000000` base is used transiently by the coreboot postcar stage.
This is deliberate temporal reuse: upload is exposed only after postcar has
transferred control to relocatable ramstage in CBMEM and cannot return. Before
any stage loader writes ordinary RAM, romstage performs one joint
transactional UC smoke over representative addresses in CBMEM and the future
postcar/object window, then restores and verifies every original dword.
Keeping all patterns resident at once detects representative aliases between
the two windows. It then tests every aligned dword in each complete 8-MiB
window with an address-derived pattern and its inverse and verifies a final
clear. This is a 16-MiB finite bring-up test, not a long-term stability proof
or validation of the rest of the DIMM.

B06V6 validates the exact CBMEM allocation and MTRR state again in ramstage.
CBMEM, including relocated ramstage, is WB; the postcar/object window remains
UC in this first image. The linked ramstage memory size must remain smaller
than the 8-MiB CBMEM reservation in every released build.

## Object header

Every integer is unsigned and little endian. The header is exactly 32 bytes.
CRC32 means CRC-32/ISO-HDLC (reflected polynomial `0xedb88320`, initial and
final XOR `0xffffffff`), matching Python `binascii.crc32` and the standard
`123456789 -> cbf43926` check vector.

| Offset | Size | Field | Rule |
| ---: | ---: | --- | --- |
| `00` | 4 | magic | ASCII `XRL1` |
| `04` | 1 | version | `01` |
| `05` | 1 | header size | `20` hex |
| `06` | 2 | flags | only bits 0 and 1 |
| `08` | 4 | object ID | nonzero operator/host nonce |
| `0c` | 4 | destination | 16-byte aligned physical address |
| `10` | 4 | payload length | nonzero and policy bounded |
| `14` | 4 | entry offset | zero for data, inside payload for code |
| `18` | 4 | payload CRC32 | covers exactly `length` payload bytes |
| `1c` | 4 | header CRC32 | covers header bytes `00..1b` |

Flags:

- bit 0, `READBACK_VERIFY`: after reception, read the complete destination
  back through the sink and compare a second CRC32;
- bit 1, `EXECUTABLE`: expose the object to the separate execute gate. This
  flag is rejected unless `READBACK_VERIFY` is also set.

Unknown flags, zero object IDs, zero/oversized lengths, 32-bit wraparound,
unaligned or out-of-policy destinations, invalid entry offsets, missing sink
callbacks, and malformed header CRCs fail closed. Starting any replacement
transfer immediately invalidates the prior object's execute capability.

The absolute implementation ceiling is 4 MiB. A firmware integration can and
should impose a smaller limit. The implementation uses no heap and writes
chunks directly to the validated sink. It must not be called before the exact
MINIT/DRAM gate has passed.

Sink reads and writes must be all-or-nothing. An aborted transfer invalidates
its metadata and execute capability but deliberately does not erase bytes
already written to the staging window; callers must treat the entire window as
untrusted scratch space until another object completes successfully.

## UART exchange

ROMMON owns the human-readable command that enters binary mode. The matching
host tool currently expects `ramload` followed by carriage return. Once binary
mode begins, neither side uses line echo:

```text
host                         firmware
ramload\r              ->
                       <-  READY status
32-byte XRL1 header    ->
                       <-  HEADER_ACK or NAK
exact payload bytes    ->
                       <-  COMPLETE_ACK or NAK
```

Every firmware status is a 20-byte little-endian frame:

| Offset | Size | Field |
| ---: | ---: | --- |
| `00` | 4 | ASCII `XRA1` |
| `04` | 1 | version `01` |
| `05` | 1 | status code |
| `06` | 2 | reserved zero |
| `08` | 4 | object ID, or zero before a header exists |
| `0c` | 4 | phase-specific detail |
| `10` | 4 | CRC32 of bytes `00..0f` |

ACK status codes are `10` READY, `11` HEADER_ACK, and `12` COMPLETE_ACK.
READY detail is the firmware's maximum accepted object size. HEADER_ACK detail
is the accepted payload length. COMPLETE_ACK detail is the accepted payload
CRC32. A NAK is `80 + enum x58_rl_result`; its detail is the number of payload
bytes accepted so far.

The firmware uses separate nonzero per-byte poll limits for header and
payload. UART framing/overrun/parity faults and timeouts abort the object and
clear its execute capability. All transmitted status bytes use a caller
provided bounded UART primitive. A failed ACK transmission also invalidates
the object because the host cannot know whether the transfer committed.
Beginning a new serial receive attempt clears the previous object capability,
header, byte count, and running CRC before transmitting READY. Consequently a
READY transmission failure cannot leave old code armed, and a header timeout
reports a zero accepted-byte count rather than stale transfer metadata.

At 115200 8N1, a 4-MiB payload needs at least about 364 seconds on the wire;
the host completion timeout must account for transfer size and the full DRAM
read-back pass. A smaller initial limit is operationally preferable.

## Transport-independent ingestion

`x58_rl_begin()`, `x58_rl_write()`, and `x58_rl_finish()` form the common sink
API. A later TFTP implementation should:

1. receive or provision the exact same 32-byte `XRL1` header;
2. call `begin` once;
3. feed each in-order TFTP data block to `write` without gaps or duplicates;
4. call `finish` only after exactly the declared byte count;
5. return ACK only after the payload and optional read-back CRCs pass.

The TFTP layer remains responsible for block-number handling, duplicate block
ACKs, retransmission deadlines, server identity, and network packet bounds.
It must never call `write` twice for a retransmitted block. The sink makes no
claim about packet authenticity; CRC32 detects accidental corruption, not a
malicious sender. A physically isolated lab network and explicit local
operator arming are required.

Data objects can hold register tables, test patterns, logs, relocatable debug
payloads, or locally supplied proprietary research bytes. Vendor bytes remain
local and must never be committed or incorporated into a distributable ROM.
Loading a vendor module as data does not establish that it is directly
callable; its entry ABI, dependencies, fixed addresses, PEI services, state,
and reset behavior still have to be reconstructed independently.

## Execute gate and 32-bit ABI

Reception never executes an object. Raw execution requires all of the
following:

1. an object received with both `EXECUTABLE` and `READBACK_VERIFY`;
2. a distinct arm operation containing the exact object ID and payload CRC32;
3. a later execute operation repeating that exact pair;
4. a complete additional read-back CRC immediately before releasing the entry
   address.

The execute capability is consumed before validation and therefore before
control can pass to uploaded code. A mismatch, cancellation, replacement
upload, failed CRC, or failed sink read requires a new arm. A failed final CRC
also invalidates the loaded object.

The firmware integration—not the transport core—performs the indirect call.
The v1 ABI is 32-bit x86 cdecl:

```c
uint32_t entry(const struct x58_rl_exec_context_v1 *context);
```

The entry address is `destination + entry_offset`. The context contains its
magic/version/size, object ID, image base and length, entry offset, payload
CRC32, and a zero service-table field. Uploaded code runs at firmware
privilege on the caller's current stack. The loader does not establish paging,
an IDT, exception recovery, ABI-compatible relocations, a private stack, or a
return watchdog. A fault, reset, bad stack, disabled UART, overwritten
firmware data, or non-returning loop cannot be recovered by the loader.
Before an eventual indirect call, the x86 integration must also execute the
appropriate instruction-fetch serialization after stores to the loaded image;
the transport core intentionally does not make cache-policy assumptions.

For B06V6, upload, status, hash, bounded dump, and relocation inspection are
available, but the indirect call is deliberately unavailable. The ramstage
link contains no reference to `x58_rl_arm_execute()` or
`x58_rl_prepare_execute()`, so section garbage collection omits both routines
from the final stage. A later build must first verify a private stack,
instruction-fetch serialization, exception handling and a tiny known-returning
payload. It must retain the serial diagnostic path and socketed recovery chip.

## Host tool

Create a deterministic container with an explicit object ID:

```bash
python3 scripts/x58_ram_loader.py pack debug.bin \
  --address 0x02000000 --object-id 0x12345678 \
  --output /tmp/debug.xrl
python3 scripts/x58_ram_loader.py inspect /tmp/debug.xrl
```

Send a data object over COM1's USB adapter:

```bash
python3 scripts/x58_ram_loader.py send debug.bin \
  --port /dev/ttyUSB1 --baud 115200 --address 0x02000000
```

For a future raw executable integration, add
`--executable --entry-offset OFFSET`. B06V6 can store and inspect such a marked
object but exposes no arm/execute command. The `send` subcommand uses
`pyserial` when installed and otherwise a built-in POSIX `termios` backend;
pack, inspect, tests and Linux/ConsolePi transfer therefore need no additional
Python package. Both backend writes and the POSIX output drain are bounded by
deadlines, including when a USB-UART disappears. Do not run picocom and the
uploader on the same TTY.

## Verification

Portable tests, independent of coreboot or proprietary data:

```bash
cc -std=c11 -Wall -Wextra -Werror \
  -Icoreboot/src/mainboard/msi/x58_pro_e \
  coreboot/src/mainboard/msi/x58_pro_e/ram_loader.c \
  scripts/test_x58_ram_loader.c -o /tmp/test_x58_ram_loader
/tmp/test_x58_ram_loader
python3 scripts/test_x58_ram_loader_tool.py
```

These cover the standard CRC vector, header round trips, malformed headers,
policy ranges and alignment, incomplete/overflowing transfers, source and
read-back CRC failures, sink failures, UART ACK/NAK framing, timeout/fault
paths, fragmented host reads, executable requirements, one-shot arming,
metadata mismatches, and mutation detected by the final pre-execute CRC.

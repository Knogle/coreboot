/* SPDX-License-Identifier: GPL-2.0-only */
/* First EDK2 trial: reuse the qualified bounded opcode13/controller restore
 * path for a separate immutable payload FFS body. CRC32 is an integrity
 * witness, not authenticated boot. No write/erase/unlock or PSP command.
 */
#include <commonlib/bsd/compiler.h>
#include "native84_profile.h"
#include "native253_payload.h"
#include <amdblocks/lpc.h>
#include <amdblocks/spi.h>
#include <console/console.h>
#include <console/uart.h>
#include <device/pci_ops.h>
#include <spi-generic.h>
#include <timer.h>

static void refuse(const char *reason) __noreturn;
static void refuse(const char *reason)
{
	printk(BIOS_ERR, "NATIVE253 SPI PAYLOAD REFUSAL %s\n", reason);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

uint32_t native253_crc32(const void *data, size_t bytes)
{
	const volatile uint8_t *p = data;
	uint32_t crc = UINT32_MAX;

	for (size_t i = 0; i < bytes; i++) {
		crc ^= p[i];
		for (unsigned int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
	}
	return ~crc;
}

void native253_load_payload(void *destination)
{
	const pci_devfn_t lpc = PCI_DEV(0, 0x14, 3);
	const uint16_t owner_mask = SPI_MUTEX_PSP_OWNS | SPI_MUTEX_HFP_OWNS |
		SPI_MUTEX_HBIOS_OWNS | SPI_SEMAPHORE_DRIVER_LOCKED |
		SPI_SEMAPHORE_BIOS_LOCKED;
	struct spi_slave slave;
	struct stopwatch bound;
	uint8_t fifo[68], opcode, tx, rx, cs, override, addr32, page;
	uint16_t enable, speed, misc;
	uint32_t restricted[2], status, completed = 0;
	int ret = 0, restore = 0, controls_same, match = 0;

	/* This proposal must not silently acquire new hardware/SMM owners. */
	_Static_assert(CONFIG(SOC_AMD_COMMON_BLOCK_SPI), "genuine FCH SPI provider");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_SPI_SEMAPHORE),
		"no new hardware-mutex writes in this fixed trial");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_PSP_SMI),
		"no new PSP/SMM semaphore writes in this fixed trial");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_PSP_ROM_ARMOR3),
		"no ROM Armor/mailbox change in this fixed trial");
	_Static_assert(CONFIG(HAVE_MONOTONIC_TIMER), "existing romstage timer");
	_Static_assert(NATIVE253_SPI_BYTES > 0 && NATIVE253_SPI_BYTES % 64 == 0,
		"bounded payload and finite64-byte transfers");
	_Static_assert((!CONFIG(B565D4_LARGE_PAYLOAD) &&
		NATIVE253_SPI_OFFSET == 0x01ec1000U && NATIVE253_SPI_BYTES <= 0x13efc0U) ||
		(CONFIG(B565D4_LARGE_PAYLOAD) &&
		NATIVE253_SPI_OFFSET == 0x01a5e000U && NATIVE253_SPI_BYTES <= 0x3014c0U),
		"independent physical payload raw FFS body");

	printk(BIOS_INFO, "NATIVE253 SPI13 PAYLOAD BEGIN offset=%08x bytes=%u\n", NATIVE253_SPI_OFFSET, NATIVE253_SPI_BYTES);
	uart_tx_flush(1);
	if (pci_s_read_config32(lpc, 0) != 0x790e1022 ||
	    pci_s_read_config32(lpc, 0xa0) != 0xfec10002)
		refuse("SPI13 fixed LPC identity/base/decode");
	for (unsigned int i = 0; i < MAX_ROM_PROTECT_RANGES; i++) {
		uint32_t range = pci_s_read_config32(lpc, ROM_PROTECT_RANGE_REG(i));
		printk(BIOS_INFO, "NATIVE253 SPI13 LPC RP%u=%08x\n", i, range);
		if (range & ROM_RANGE_RP) /* RP bit9; RANGE_UNIT bit8 is not protection. */
			refuse("SPI13 read protection; no unlock");
	}
	/* spi_init discovers current base/boot CS; it is not fch_spi_early_init. */
	spi_init();
	if (spi_get_bar() != 0xfec10000U)
		refuse("SPI13 discovered base");
	cs = spi_read8(0x1d);
	enable = spi_read16(SPI100_ENABLE);
	speed = spi_read16(SPI100_SPEED_CONFIG);
	override = spi_read8(SPI_ROM2_OVERRIDE);
	addr32 = spi_read8(SPI_ROM_ADDR32_CTRL0);
	page = spi_read8(SPI_ROM_PAGE);
	misc = spi_read16(SPI_MISC_CNTRL);
	restricted[0] = spi_read32(0x04);
	restricted[1] = spi_read32(0x08);
	status = spi_read32(SPI_STATUS);
	printk(BIOS_INFO, "NATIVE253 SPI13 PREFLIGHT ctrl=%08x status=%08x "
		"CS=%02x enable=%04x speed=%04x override=%02x addr32=%02x page=%02x "
		"misc=%04x restricted=%08x/%08x\n", spi_read32(SPI_CNTRL0), status,
		cs, enable, speed, override, addr32, page, misc,
		restricted[0], restricted[1]);
	uart_tx_flush(1);
	if (status == UINT32_MAX || (status & SPI_BUSY) || (cs & 3) ||
	    !(enable & SPI_USE_SPI100) || (misc & owner_mask))
		refuse("SPI13 fresh idle/CS0/SPI100/observed owners");
	for (unsigned int i = 0; i < 8; i++)
		if (((restricted[i / 4] >> (8 * (i % 4))) & 0xff) == 0x13)
			refuse("SPI13 restricted opcode; no unlock");
	if (spi_setup_slave(0, 0, &slave))
		refuse("SPI13 genuine controller setup");
	opcode = spi_read8(0x45);
	tx = spi_read8(0x48);
	rx = spi_read8(0x4b);
	for (unsigned int i = 0; i < sizeof(fifo); i++)
		fifo[i] = spi_read8(SPI_FIFO + i);
	if (spi_claim_bus(&slave))
		refuse("SPI13 genuine controller claim");

	stopwatch_init_msecs_expire(&bound, 15000);
	for (uint32_t off = 0; off < NATIVE253_SPI_BYTES; off += 64) {
		uint32_t address = NATIVE253_SPI_OFFSET + off;
		uint8_t cmd[5] = { 0x13, address >> 24, address >> 16,
			address >> 8, address };
		/* Same command/response vectors as genuine do_spi_flash_cmd().
		 * FCH combines them into one opcode13/TX4/RX64 transaction.
		 */
		struct spi_op vectors[2] = {
			{ .dout = cmd, .bytesout = sizeof(cmd) },
			{ .din = (uint8_t *)destination + off, .bytesin = 64 },
		};
		if (stopwatch_expired(&bound)) {
			ret = -1;
			break;
		}
		ret = spi_xfer_vector(&slave, vectors, 2);
		if (ret || vectors[0].status != SPI_OP_SUCCESS ||
		    vectors[1].status != SPI_OP_SUCCESS)
			break;
		completed += 64;
	}
	if (stopwatch_expired(&bound))
		ret = -1;

	/* Restore only owned command/count/FIFO bytes, never trigger/status or CS.
	 * No retry/reset if completion is still Busy or unknown.
	 */
	status = spi_read32(SPI_STATUS);
	if (status != UINT32_MAX && !(status & SPI_BUSY)) {
		spi_write8(0x45, opcode);
		spi_write8(0x48, tx);
		spi_write8(0x4b, rx);
		for (unsigned int i = 0; i < sizeof(fifo); i++)
			spi_write8(SPI_FIFO + i, fifo[i]);
		restore = spi_read8(0x45) == opcode && spi_read8(0x48) == tx &&
			spi_read8(0x4b) == rx;
		for (unsigned int i = 0; i < sizeof(fifo); i++)
			restore &= spi_read8(SPI_FIFO + i) == fifo[i];
	}
	controls_same = spi_read8(0x1d) == cs &&
		spi_read16(SPI100_ENABLE) == enable &&
		spi_read16(SPI100_SPEED_CONFIG) == speed &&
		spi_read8(SPI_ROM2_OVERRIDE) == override &&
		spi_read8(SPI_ROM_ADDR32_CTRL0) == addr32 &&
		spi_read8(SPI_ROM_PAGE) == page &&
		spi_read16(SPI_MISC_CNTRL) == misc;
	/* With the static config above and unchanged CS0 this release performs
	 * no CS, hardware-mutex or SMM semaphore write. Refuse instead of repairing
	 * an unexpected chip-select change by another agent.
	 */
	if (spi_read8(0x1d) == cs)
		spi_release_bus(&slave);
	else
		controls_same = 0;
	if (!ret && completed == NATIVE253_SPI_BYTES && restore && controls_same) {
		match = native253_crc32(destination, SELF_BYTES) == NATIVE253_SELF_CRC32;
		for (uint32_t i = SELF_BYTES; i < NATIVE253_SPI_BYTES; i++)
			match &= ((const uint8_t *)destination)[i] == 0xff;
	}

	printk(BIOS_INFO, "NATIVE253 SPI13 RESULT completed_bytes=%u ret=%d status=%08x "
		"owned_restore=%d controls_same=%d whole_SELF_crc32_match=%d\n",
		completed, ret, status, restore, controls_same, match);
	uart_tx_flush(1);
	if (ret || completed != NATIVE253_SPI_BYTES ||
	    !restore || !controls_same || !match)
		refuse("SPI13 payload incomplete/CRC32 mismatch; no fallback/retry");
	printk(BIOS_INFO, "NATIVE253 SPI13 PAYLOAD PASS bytes=%u crc32=%08x\n",
		SELF_BYTES, NATIVE253_SELF_CRC32);
	uart_tx_flush(1);
}

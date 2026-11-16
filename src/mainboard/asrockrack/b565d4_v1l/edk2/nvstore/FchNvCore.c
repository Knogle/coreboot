/* SPDX-License-Identifier: BSD-2-Clause-Patent */

#include "FchNvCore.h"

#define SPI_STATUS 0x4c
#define SPI_MISC 0xfc
#define SPI_FIFO 0x80
#define SPI_BUSY (1U << 31)
#define SPI_OTHER_OWNERS ((1U << 10) | (1U << 9) | (1U << 4) | (1U << 3))
#define SPI_BIOS_OWNER (1U << 5)
#define FIFO_BYTES 68

struct saved_controller {
	uint8_t fifo[FIFO_BYTES];
	uint8_t opcode, tx, rx, cs;
	uint16_t enable, speed;
};

/* Serialized by the adapter lock. Count command time as well as delays. */
static uint64_t operation_start;
static unsigned int operation_timeout;

static int range_valid(uint32_t offset, size_t bytes)
{
	return offset < FCH_NV_BYTES && bytes && bytes <= FCH_NV_BYTES - offset;
}

static enum fch_nv_result wait_controller(void)
{
	uint64_t start = fch_nv_io_time_us();

	for (;;) {
		uint64_t now = fch_nv_io_time_us();

		if (now - start >= 500000 || now - operation_start >= operation_timeout)
			return FCH_NV_TIMEOUT;
		uint32_t status = fch_nv_io_read32(SPI_STATUS);

		if (status == UINT32_MAX)
			return FCH_NV_IO_ERROR;
		if (!(status & SPI_BUSY))
			return FCH_NV_OK;
		fch_nv_io_delay(10);
	}
}

static enum fch_nv_result command(uint8_t opcode, const uint8_t *tx, size_t tx_bytes,
	uint8_t *rx, size_t rx_bytes)
{
	if (tx_bytes + rx_bytes > FIFO_BYTES)
		return FCH_NV_INVALID;
	enum fch_nv_result result = wait_controller();

	if (result != FCH_NV_OK)
		return result;
	fch_nv_io_write8(0x45, opcode);
	fch_nv_io_write8(0x48, tx_bytes);
	fch_nv_io_write8(0x4b, rx_bytes);
	for (size_t i = 0; i < tx_bytes; i++)
		fch_nv_io_write8(SPI_FIFO + i, tx[i]);
	fch_nv_io_write8(0x47, 0x80);
	result = wait_controller();
	if (result == FCH_NV_OK)
		for (size_t i = 0; i < rx_bytes; i++)
			rx[i] = fch_nv_io_read8(SPI_FIFO + tx_bytes + i);
	return result;
}

static enum fch_nv_result begin(struct saved_controller *saved)
{
	if (!fch_nv_io_lock())
		return FCH_NV_BUSY;
	operation_start = fch_nv_io_time_us();
	operation_timeout = 6000000;
	uint16_t misc = fch_nv_io_read16(SPI_MISC);
	uint32_t status = fch_nv_io_read32(SPI_STATUS);

	if ((misc & (SPI_OTHER_OWNERS | SPI_BIOS_OWNER)) ||
	    status == UINT32_MAX || (status & SPI_BUSY) ||
	    (fch_nv_io_read8(0x1d) & 3) || !(fch_nv_io_read16(0x20) & 1)) {
		fch_nv_io_unlock();
		return FCH_NV_BUSY;
	}
	/* Follow the AMD controller's Host-BIOS mutex protocol. No ownership
	 * override, SMI or PSP command is used. A losing acquisition is released.
	 */
	fch_nv_io_write16(SPI_MISC, misc | SPI_BIOS_OWNER);
	misc = fch_nv_io_read16(SPI_MISC);
	if (!(misc & SPI_BIOS_OWNER) || (misc & SPI_OTHER_OWNERS)) {
		if (misc & SPI_BIOS_OWNER)
			fch_nv_io_write16(SPI_MISC, misc & ~SPI_BIOS_OWNER);
		fch_nv_io_unlock();
		return FCH_NV_BUSY;
	}
	saved->opcode = fch_nv_io_read8(0x45);
	saved->tx = fch_nv_io_read8(0x48);
	saved->rx = fch_nv_io_read8(0x4b);
	saved->cs = fch_nv_io_read8(0x1d);
	saved->enable = fch_nv_io_read16(0x20);
	saved->speed = fch_nv_io_read16(0x22);
	for (unsigned int i = 0; i < FIFO_BYTES; i++)
		saved->fifo[i] = fch_nv_io_read8(SPI_FIFO + i);
	return FCH_NV_OK;
}

static enum fch_nv_result finish(const struct saved_controller *saved,
	enum fch_nv_result result)
{
	uint32_t status = fch_nv_io_read32(SPI_STATUS);

	/* Never overwrite command/FIFO state while an unknown transaction is
	 * still active. There is no controller reset or automatic operation retry.
	 */
	if (status == UINT32_MAX || (status & SPI_BUSY)) {
		fch_nv_io_unlock();
		return FCH_NV_IO_ERROR;
	}
	fch_nv_io_write8(0x45, saved->opcode);
	fch_nv_io_write8(0x48, saved->tx);
	fch_nv_io_write8(0x4b, saved->rx);
	for (unsigned int i = 0; i < FIFO_BYTES; i++)
		fch_nv_io_write8(SPI_FIFO + i, saved->fifo[i]);
	if (fch_nv_io_read8(0x45) != saved->opcode ||
	    fch_nv_io_read8(0x48) != saved->tx || fch_nv_io_read8(0x4b) != saved->rx ||
	    fch_nv_io_read8(0x1d) != saved->cs ||
	    fch_nv_io_read16(0x20) != saved->enable ||
	    fch_nv_io_read16(0x22) != saved->speed)
		result = FCH_NV_IO_ERROR;
	for (unsigned int i = 0; i < FIFO_BYTES; i++)
		if (fch_nv_io_read8(SPI_FIFO + i) != saved->fifo[i])
			result = FCH_NV_IO_ERROR;
	uint16_t misc = fch_nv_io_read16(SPI_MISC);

	if (!(misc & SPI_BIOS_OWNER) || (misc & SPI_OTHER_OWNERS))
		result = FCH_NV_IO_ERROR;
	fch_nv_io_write16(SPI_MISC, misc & ~SPI_BIOS_OWNER);
	if (fch_nv_io_read16(SPI_MISC) & SPI_BIOS_OWNER)
		result = FCH_NV_IO_ERROR;
	fch_nv_io_unlock();
	return result;
}

static void address_bytes(uint8_t *data, uint32_t offset)
{
	uint32_t address = FCH_NV_OFFSET + offset;

	data[0] = address >> 24;
	data[1] = address >> 16;
	data[2] = address >> 8;
	data[3] = address;
}

static enum fch_nv_result read_chunk(uint32_t offset, uint8_t *data, size_t bytes)
{
	uint8_t address[4];

	address_bytes(address, offset);
	return command(0x13, address, sizeof(address), data, bytes);
}

enum fch_nv_result fch_nv_probe(void)
{
	struct saved_controller saved;
	enum fch_nv_result result = begin(&saved);
	uint8_t id[3], sr;
	static const uint8_t used[] = { 0x04, 0x05, 0x06, 0x12, 0x13, 0x21, 0x9f };

	if (result != FCH_NV_OK)
		return result;
	for (unsigned int i = 0; i < 8; i++) {
		uint8_t restricted = fch_nv_io_read8(4 + i);

		for (unsigned int j = 0; j < sizeof(used); j++)
			if (restricted == used[j])
				return finish(&saved, FCH_NV_IO_ERROR);
	}
	result = command(0x9f, NULL, 0, id, sizeof(id));
	if (result == FCH_NV_OK && (id[0] != 0xef || id[1] != 0x40 || id[2] != 0x19))
		result = FCH_NV_IO_ERROR;
	if (result == FCH_NV_OK)
		result = command(0x05, NULL, 0, &sr, 1);
	if (result == FCH_NV_OK && sr)
		result = FCH_NV_IO_ERROR;
	return finish(&saved, result);
}

enum fch_nv_result fch_nv_read(uint32_t offset, void *data, size_t bytes)
{
	struct saved_controller saved;
	uint8_t *out = data;

	if (!out || !range_valid(offset, bytes))
		return FCH_NV_INVALID;
	enum fch_nv_result result = begin(&saved);

	if (result != FCH_NV_OK)
		return result;
	for (size_t done = 0; done < bytes && result == FCH_NV_OK;) {
		size_t chunk = bytes - done > 64 ? 64 : bytes - done;

		result = read_chunk(offset + done, out + done, chunk);
		done += chunk;
	}
	return finish(&saved, result);
}

#ifndef FCH_NV_CAPSULE_READ_ONLY
static enum fch_nv_result write_enable(void)
{
	uint8_t sr;
	enum fch_nv_result result = command(0x06, NULL, 0, NULL, 0);

	if (result == FCH_NV_OK)
		result = command(0x05, NULL, 0, &sr, 1);
	if (result == FCH_NV_OK && (sr & 3) != 2)
		result = FCH_NV_IO_ERROR;
	return result;
}

static enum fch_nv_result wait_flash(unsigned int timeout_us)
{
	uint64_t start = fch_nv_io_time_us();

	while (fch_nv_io_time_us() - start < timeout_us) {
		uint8_t sr;
		enum fch_nv_result result = command(0x05, NULL, 0, &sr, 1);

		if (result != FCH_NV_OK)
			return result;
		if (!(sr & 1))
			return (sr & 2) ? FCH_NV_IO_ERROR : FCH_NV_OK;
		fch_nv_io_delay(10);
	}
	return FCH_NV_TIMEOUT;
}

enum fch_nv_result fch_nv_write(uint32_t offset, const void *data, size_t bytes,
	size_t *completed)
{
	struct saved_controller saved;
	const uint8_t *in = data;
	uint8_t buffer[68], verify[64];

	if (completed)
		*completed = 0;
	if (!in || !completed || !range_valid(offset, bytes))
		return FCH_NV_INVALID;
	enum fch_nv_result result = begin(&saved);

	if (result != FCH_NV_OK)
		return result;
	/* Validate the complete operation before its first write-enable. */
	for (size_t done = 0; done < bytes && result == FCH_NV_OK;) {
		size_t chunk = bytes - done > 64 ? 64 : bytes - done;

		result = read_chunk(offset + done, verify, chunk);
		for (size_t i = 0; i < chunk && result == FCH_NV_OK; i++)
			if ((verify[i] & in[done + i]) != in[done + i])
				result = FCH_NV_NEEDS_ERASE;
		done += chunk;
	}
	while (*completed < bytes && result == FCH_NV_OK) {
		uint32_t at = offset + *completed;
		size_t chunk = bytes - *completed;

		if (chunk > 64)
			chunk = 64;
		if (chunk > 256 - (at & 255))
			chunk = 256 - (at & 255);
		address_bytes(buffer, at);
		for (size_t i = 0; i < chunk; i++)
			buffer[4 + i] = in[*completed + i];
		result = write_enable();
		if (result == FCH_NV_OK)
			result = command(0x12, buffer, 4 + chunk, NULL, 0);
		if (result == FCH_NV_OK)
			result = wait_flash(20000);
		if (result == FCH_NV_OK)
			result = read_chunk(at, verify, chunk);
		for (size_t i = 0; i < chunk && result == FCH_NV_OK; i++)
			if (verify[i] != in[*completed + i])
				result = FCH_NV_IO_ERROR;
		if (result == FCH_NV_OK)
			*completed += chunk;
	}
	return finish(&saved, result);
}

enum fch_nv_result fch_nv_erase(uint32_t offset)
{
	struct saved_controller saved;
	uint8_t address[4], verify[64];

	if ((offset & (FCH_NV_BLOCK - 1)) || !range_valid(offset, FCH_NV_BLOCK))
		return FCH_NV_INVALID;
	enum fch_nv_result result = begin(&saved);

	if (result != FCH_NV_OK)
		return result;
	address_bytes(address, offset);
	result = write_enable();
	if (result == FCH_NV_OK)
		result = command(0x21, address, sizeof(address), NULL, 0);
	if (result == FCH_NV_OK)
		result = wait_flash(5000000);
	for (unsigned int done = 0; done < FCH_NV_BLOCK && result == FCH_NV_OK; done += 64) {
		result = read_chunk(offset + done, verify, sizeof(verify));
		for (unsigned int i = 0; i < sizeof(verify) && result == FCH_NV_OK; i++)
			if (verify[i] != 0xff)
				result = FCH_NV_IO_ERROR;
	}
	return finish(&saved, result);
}
#endif /* !FCH_NV_CAPSULE_READ_ONLY */

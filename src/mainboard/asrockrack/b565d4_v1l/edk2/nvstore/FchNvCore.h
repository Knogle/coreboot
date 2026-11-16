/* SPDX-License-Identifier: BSD-2-Clause-Patent */

#ifndef B565D4_FCH_NV_CORE_H
#define B565D4_FCH_NV_CORE_H

#ifdef FCH_NV_EDK2
#include <Base.h>
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef UINTN size_t;
#define UINT32_MAX MAX_UINT32
#else
#include <stddef.h>
#include <stdint.h>
#endif

/* The ordinary runtime provider remains limited to its allocated NVAR.
 * Capsule readers and any future explicitly selected writer use a separate
 * DXE image and the complete physical chip address space.
 */
#if defined(FCH_NV_CAPSULE_READ_ONLY) && defined(FCH_NV_CAPSULE_UPDATE)
#error Capsule SPI cannot be both read-only and update-enabled
#endif
#if defined(FCH_NV_CAPSULE_READ_ONLY) || defined(FCH_NV_CAPSULE_UPDATE)
#define FCH_NV_OFFSET 0x00000000U
#define FCH_NV_BYTES  0x02000000U
#else
#define FCH_NV_OFFSET 0x01037000U
#define FCH_NV_BYTES  0x00020000U
#endif
#define FCH_NV_BLOCK  0x00001000U

enum fch_nv_result {
	FCH_NV_OK,
	FCH_NV_INVALID,
	FCH_NV_BUSY,
	FCH_NV_IO_ERROR,
	FCH_NV_TIMEOUT,
	FCH_NV_NEEDS_ERASE,
};

/* The platform adapter supplies bounded MMIO and a nonblocking CPU lock.
 * It maps the controller as runtime MMIO and converts that pointer on VA
 * transition. The algorithm has no embedded physical/virtual RAM pointers.
 */
uint8_t fch_nv_io_read8(unsigned int offset);
uint16_t fch_nv_io_read16(unsigned int offset);
uint32_t fch_nv_io_read32(unsigned int offset);
void fch_nv_io_write8(unsigned int offset, uint8_t value);
void fch_nv_io_write16(unsigned int offset, uint16_t value);
void fch_nv_io_delay(unsigned int microseconds);
uint64_t fch_nv_io_time_us(void);
int fch_nv_io_lock(void);
void fch_nv_io_unlock(void);

enum fch_nv_result fch_nv_probe(void);
enum fch_nv_result fch_nv_read(uint32_t offset, void *data, size_t bytes);
enum fch_nv_result fch_nv_write(uint32_t offset, const void *data, size_t bytes,
	size_t *completed);
enum fch_nv_result fch_nv_erase(uint32_t offset);

#endif

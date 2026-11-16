/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef B565D4_FCH_CAPSULE_POLICY_H
#define B565D4_FCH_CAPSULE_POLICY_H

#ifdef FCH_NV_EDK2
#include <Base.h>
typedef UINT8 capsule_u8;
typedef UINT32 capsule_u32;
typedef UINTN capsule_size;
#else
#include <stddef.h>
#include <stdint.h>
typedef uint8_t capsule_u8;
typedef uint32_t capsule_u32;
typedef size_t capsule_size;
#endif

#define B565D4_CAPSULE_BYTES 0x02000000U
#define B565D4_CAPSULE_BANK_BYTES 0x01000000U
#define B565D4_CAPSULE_SECTOR_BYTES 0x00001000U

enum b565d4_capsule_result {
	B565D4_CAPSULE_OK,
	B565D4_CAPSULE_BAD_INPUT,
	B565D4_CAPSULE_BAD_FMAP,
	B565D4_CAPSULE_BAD_MIRROR,
	B565D4_CAPSULE_IMMUTABLE_CHANGE,
};

struct b565d4_capsule_audit {
	capsule_u32 first_bad_offset;
	capsule_u32 changed_bytes[6];
};

/* This policy performs no I/O. The caller must obtain a complete fresh SPI
 * read and must not expose a flash-write API until validation succeeds.
 */
enum b565d4_capsule_result b565d4_capsule_validate(
	const capsule_u8 *current, const capsule_u8 *candidate,
	capsule_size bytes, struct b565d4_capsule_audit *audit);

/* Make one exact 4-KiB sector, preserving immutable edge bytes and stores.
 * The caller must validate the complete pair of images first.
 */
int b565d4_capsule_merge_sector(const capsule_u8 *current,
	const capsule_u8 *candidate, capsule_size bytes, capsule_u32 sector,
	capsule_u8 *merged);

/* Defense in depth for the final SPI callback: reject every changed byte
 * outside the six qualified windows, even if its caller is changed later.
 */
int b565d4_capsule_sector_delta_allowed(capsule_u32 sector,
	const capsule_u8 *before, const capsule_u8 *after);

#endif

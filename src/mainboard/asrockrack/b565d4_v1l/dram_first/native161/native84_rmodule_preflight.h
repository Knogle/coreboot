/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_RMODULE_PREFLIGHT_H
#define NATIVE84_RMODULE_PREFLIGHT_H

#include <stdint.h>

#define NATIVE84_RMODULE_HEADER_BYTES 64u
#define NATIVE84_RMODULE_DESCRIPTOR_BYTES 56u
#define NATIVE84_RMODULE_MAX_PROTECTED 32u

struct native84_rmodule_interval {
	uint32_t begin;
	uint32_t end;
};

struct native84_rmodule_request {
	const uint8_t *source;
	uint32_t source_bytes;
	uint32_t destination;
	/* The isolated module allocation, not the containing CBMEM arena. */
	struct native84_rmodule_interval allocation;
	const void *descriptor;
	uint32_t descriptor_bytes;
	const struct native84_rmodule_interval *protected;
	uint32_t protected_count;
};

struct native84_rmodule_facts {
	uint8_t header[NATIVE84_RMODULE_HEADER_BYTES];
	struct native84_rmodule_interval source;
	struct native84_rmodule_interval allocation;
	struct native84_rmodule_interval program;
	struct native84_rmodule_interval payload;
	/* Serialized source table; it may overlap the destination BSS in-place. */
	struct native84_rmodule_interval relocations;
	struct native84_rmodule_interval bss;
	struct native84_rmodule_interval parameters;
	uint32_t link_start;
	uint32_t link_end;
	uint32_t entry;
	uint32_t relocation_count;
	/* Stock IA32 stores (uint32_t)(encoded_DWORD + relocation_adjustment). */
	uint32_t relocation_adjustment;
	uint32_t in_place;
};

enum native84_rmodule_result {
	NATIVE84_RMODULE_VALID = 0,
	NATIVE84_RMODULE_BAD_ARGUMENT,
	NATIVE84_RMODULE_BAD_SOURCE,
	NATIVE84_RMODULE_BAD_HEADER,
	NATIVE84_RMODULE_BAD_SERIALIZATION,
	NATIVE84_RMODULE_BAD_PROGRAM,
	NATIVE84_RMODULE_BAD_ENTRY,
	NATIVE84_RMODULE_BAD_BSS,
	NATIVE84_RMODULE_BAD_PARAMETERS,
	NATIVE84_RMODULE_BAD_RELOCATION,
	NATIVE84_RMODULE_BAD_ALIAS,
	NATIVE84_RMODULE_PROTECTED_OVERLAP,
};

/*
 * IA32/little-endian structural preflight only; no stock loader is called.
 * Caller supplies naturally aligned, backed, stable request/list/descriptor,
 * the exact immutable initialized source extent and a separate writable facts
 * object. Address checks cannot establish these mappings or their lifetimes.
 * On failure facts is untouched. No source/program/descriptor byte is written.
 * A VALID result is neither code authentication nor a reusable load capability.
 * Any future wrapper must preflight and load in one qualified immutable
 * operation, recheck this header immediately before genuine parse/load, admit
 * all stock effects/hooks, and verify loaded state before descriptor attachment.
 */
enum native84_rmodule_result native84_rmodule_preflight(
	const struct native84_rmodule_request *request,
	struct native84_rmodule_facts *facts);

#endif

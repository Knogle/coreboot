/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_PARAMETERS_H
#define NATIVE84_PARAMETERS_H

#include "native84_handoff.h"

/* Owned initialized storage, not the stock c_start(cbmem_top) argument.
 * The loader validates and copies the complete descriptor after BSS clearing.
 * Consumers must freshly validate it before any referenced-memory access.
 */
extern struct native84_handoff native84_handoff_parameters;

#endif

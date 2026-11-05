/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NORMAL_NATIVE99_H
#define NORMAL_NATIVE99_H
#include <commonlib/bsd/compiler.h>
#include <stdint.h>
void __noreturn native99_prepare_and_enter(const uint8_t *snapshot, uint32_t bytes);
#endif

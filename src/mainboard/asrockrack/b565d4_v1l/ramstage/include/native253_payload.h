/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_SPI_PAYLOAD_H
#define B565D4_SPI_PAYLOAD_H
#include <stddef.h>
#include <stdint.h>
#include "payload_contract.h"
#define SELF_BACKUP_ID UINT32_C(0x53393142)
uint32_t native253_crc32(const void *data, size_t bytes);
void native253_load_payload(void *destination);
#endif

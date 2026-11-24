/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/matisse_dxio_service.h>

enum cb_err matisse_dxio_request(uint32_t opcode, struct smu_payload *arguments)
{
	if (!arguments)
		return CB_ERR;

	arguments->msg[0] = opcode;
	return send_smu_message(SMC_MSG_DXIO, arguments);
}

enum cb_err matisse_dxio_set_link_speed(unsigned int engine_index, unsigned int speed,
				      struct smu_payload *reply)
{
	if (!reply || engine_index > 15 || !speed || speed > 4)
		return CB_ERR;
	*reply = (struct smu_payload) { .msg = { 0, 1, speed, engine_index, 0, 0 } };
	return matisse_dxio_request(0x810, reply);
}

uint8_t matisse_dxio_status(uint32_t raw)
{
	if ((raw & 0xffffff00U) == 0x300)
		return 3;
	return (uint8_t)raw;
}

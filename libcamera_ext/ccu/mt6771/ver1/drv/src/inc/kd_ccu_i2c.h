/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCU_I2C_H__
#define __CCU_I2C_H__

enum CCU_I2C_CHANNEL {
	CCU_I2C_CHANNEL_UNDEF    = 0x0,
	CCU_I2C_CHANNEL_MIN      = 0x1,
	CCU_I2C_CHANNEL_MAINCAM  = 0x1,
	CCU_I2C_CHANNEL_MAINCAM2 = 0x2,
	CCU_I2C_CHANNEL_SUBCAM   = 0x3,
	CCU_I2C_CHANNEL_MAX      = 0x4
};

struct ccu_i2c_buf_mva_ioarg {
	enum CCU_I2C_CHANNEL i2c_controller_id;
	uint32_t mva;
	uint32_t va_h;
	uint32_t va_l;
	uint32_t i2c_id;
};

#endif

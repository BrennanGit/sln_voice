// Copyright 2022-2023 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
// XMOS Public License: Version 1

#ifndef PCAL6408A_H_
#define PCAL6408A_H_

#define PCAL6408A_I2C_ADDR          0x20

#define PCAL6408A_INPUT_PORT        0x00
#define PCAL6408A_OUTPUT_PORT       0x01
#define PCAL6408A_POLARITY_INV      0x02
#define PCAL6408A_CONF              0x03
#define PCAL6408A_OUTPUT_DRIVE_STR0 0x40
#define PCAL6408A_OUTPUT_DRIVE_STR1 0x41
#define PCAL6408A_INPUT_LATCH       0x42
#define PCAL6408A_PULLUPDOWN_EN     0x43
#define PCAL6408A_PULLUPDOWN_SEL    0x44
#define PCAL6408A_INTERRUPT_MASK    0x45
#define PCAL6408A_INTERRUPT_STATUS  0x46
#define PCAL6408A_OUTPUT_PORT_CONF  0x4F

#endif /* PCAL6408A_H_ */

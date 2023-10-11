//------------------------------------------------------------------------------
/**
 * @file lib_i2c.h
 * @author charles-park (charles.park@hardkernel.com)
 * @brief I2C control library for ODROID-JIG.
 * @version 0.2
 * @date 2023-10-06
 *
 * @package apt install minicom
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
#ifndef __LIB_I2C_H__
#define __LIB_I2C_H__

//------------------------------------------------------------------------------
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

//------------------------------------------------------------------------------
extern int i2c_read        (int fd);
extern int i2c_read_byte   (int fd, int reg);
extern int i2c_read_word   (int fd, int reg);
extern int i2c_write       (int fd, int data);
extern int i2c_write_byte  (int fd, int reg, int value);
extern int i2c_write_word  (int fd, int reg, int value);
extern int i2c_set_addr    (int fd, int device_addr);
extern int i2c_open_device (const char *device_node, int device_addr);
extern int i2c_close       (int fd);
extern int i2c_open        (const char *device_node);

//------------------------------------------------------------------------------
#endif  // __LIB_I2C_H__
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

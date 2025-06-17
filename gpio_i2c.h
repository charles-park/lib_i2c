//------------------------------------------------------------------------------
/**
 * @file gpio_i2c.h
 * @author charles-park (charles.park@hardkernel.com)
 * @brief GPIO I2C control library for ODROID-JIG.
 * @version 0.2
 * @date 2024-07-16
 *
 * @package apt install minicom
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
#ifndef __GPIO_I2C_H__
#define __GPIO_I2C_H__

//------------------------------------------------------------------------------
#include <stdint.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define GPIO_I2C_MASK   0xFF00
#define GPIO_I2C_FLAG   0xFC00

#define IS_GPIO_I2C(x)  (((x & GPIO_I2C_MASK) == GPIO_I2C_FLAG) ? 1 : 0)

//------------------------------------------------------------------------------
extern void     gpio_i2c_close  (int fd);
extern int      gpio_i2c_saddr  (int fd, int device_addr);
extern int      gpio_i2c_ctrl   (int fd, struct i2c_smbus_ioctl_data *args);
extern int      gpio_i2c_open   (const char *device_info);

//------------------------------------------------------------------------------
#endif  // __GPIO_I2C_H__
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
extern int gpio_i2c_init (int scl_gpio, int sda_gpio);
extern int gpio_i2c_ctrl (struct i2c_smbus_ioctl_data *args);

//------------------------------------------------------------------------------
#endif  // __GPIO_I2C_H__
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

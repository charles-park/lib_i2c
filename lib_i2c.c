//------------------------------------------------------------------------------
/**
 * @file lib_i2c.c
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "lib_i2c.h"
#include "gpio_i2c.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// function prototype
//------------------------------------------------------------------------------
static void toupperstr          (char *p);
static int  check_i2c_mode      (const char *device_info);

static int  i2c_set_addr_gpio   (int fd, int device_addr);
static int  i2c_smbus_gpio      (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data);
static int  i2c_open_gpio       (const char *device_info);

static int  i2c_set_addr_hw     (int fd, int device_addr);
static int  i2c_smbus_hw        (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data);
static int  i2c_open_hw         (const char *device_info);

//------------------------------------------------------------------------------
int i2c_smbus_access(int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data);
int i2c_set_addr    (int fd, int device_addr);

int i2c_read        (int fd);
int i2c_read_byte   (int fd, int reg);
int i2c_read_word   (int fd, int reg);
int i2c_write       (int fd, int data);
int i2c_write_byte  (int fd, int reg, int value);
int i2c_write_word  (int fd, int reg, int value);
int i2c_close       (int fd);
int i2c_open        (const char *device_info);
int i2c_open_device (const char *device_info, int device_addr);

//------------------------------------------------------------------------------
int (*fp_i2c_set_addr)      (int fd, int device_addr) = NULL;
int (*fp_i2c_smbus_access)  (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data) = NULL;

int  I2C_Mode = eI2C_MODE_HW;
int  I2C_SLAVE_ADDR = 0;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int i2c_smbus_access (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
    return fp_i2c_smbus_access (fd, rw, command, size, data);
}

//------------------------------------------------------------------------------
int i2c_set_addr (int fd, int device_addr)
{
    return fp_i2c_set_addr (fd, device_addr);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
	int i, c = strlen(p);

	for (i = 0; i < c; i++, p++)
		*p = toupper(*p);
}

//------------------------------------------------------------------------------
static int check_i2c_mode (const char *device_info)
{
    char str[5];

    memset (str, 0, sizeof(str));
    memcpy (str, device_info, sizeof(str)-1);

    toupperstr (str);
    if (!strncmp ("GPIO", str, sizeof(str)-1))
        return eI2C_MODE_GPIO;
    if (!strncmp ("/DEV", str, sizeof(str)-1))
        return eI2C_MODE_HW;

    return -1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int i2c_set_addr_gpio (int fd, int device_addr)
{
    I2C_SLAVE_ADDR = (fd == FD_GPIO_I2C) ? (device_addr << 1) : 0;
    return 0;
}

//------------------------------------------------------------------------------
static int i2c_smbus_gpio (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args ;

    if (fd != FD_GPIO_I2C)  return -1;
    args.read_write = rw ;
    args.command    = command ;
    args.size       = size ;
    args.data       = data ;
    return gpio_i2c_ctrl (&args);
}

//------------------------------------------------------------------------------
static int i2c_open_gpio (const char *device_info)
{
    char gpio_info [64], *p;
    int scl_gpio, sda_gpio, i;

    memset (gpio_info, 0, sizeof(gpio_info));
    memcpy (gpio_info, device_info, strlen (device_info));

    if ((p = strtok (gpio_info, ",")) != NULL) {
        toupperstr (p);
        if (strncmp (p, "GPIO", sizeof("GPIO")))   return -1;

        for (i = 0, scl_gpio = 0, sda_gpio = 0; i < 2; i++ ) {
            p = strtok (NULL, ","); toupperstr (p);
            if (!strncmp (p, "SCL", sizeof("SCL"))) {
                p = strtok (NULL, ",");
                scl_gpio = atoi (p);
            }
            else if (!strncmp (p, "SDA", sizeof("SDA"))) {
                p = strtok (NULL, ",");
                sda_gpio = atoi (p);
            }
        }
        if (!scl_gpio || !sda_gpio)     return -1;
    }

    fp_i2c_smbus_access    = i2c_smbus_gpio;
    fp_i2c_set_addr        = i2c_set_addr_gpio;

    return gpio_i2c_init (scl_gpio, sda_gpio);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int i2c_set_addr_hw (int fd, int device_addr)
{
    if (ioctl (fd, I2C_SLAVE, device_addr) < 0) {
        fprintf (stderr, "Can't setup device : device adddr is 0x%02x\n", device_addr);
        return -1;
    }
    return 0;
}

//------------------------------------------------------------------------------
static int i2c_smbus_hw (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args ;

    args.read_write = rw ;
    args.command    = command ;
    args.size       = size ;
    args.data       = data ;
    return ioctl (fd, I2C_SMBUS, &args) ;
}

//------------------------------------------------------------------------------
static int i2c_open_hw (const char *device_info)
{
    int fd;
    if ((fd = open (device_info, O_RDWR)) < 0) {
        fprintf (stderr, "%s : Unable to open I2C device : %s\n", __func__, device_info);
        return -1;
    }
    fp_i2c_smbus_access    = i2c_smbus_hw;
    fp_i2c_set_addr        = i2c_set_addr_hw;
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int i2c_read (int fd)
{
    union i2c_smbus_data data ;

    if (i2c_smbus_access (fd, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))
        return -1 ;
    else
        return data.byte & 0xFF ;
}

//------------------------------------------------------------------------------
int i2c_read_byte (int fd, int reg)
{
    union i2c_smbus_data data;

    if (i2c_smbus_access (fd, I2C_SMBUS_READ, reg, I2C_SMBUS_BYTE_DATA, &data))
        return -1 ;
    else
        return data.byte & 0xFF ;
}

//------------------------------------------------------------------------------
int i2c_read_word (int fd, int reg)
{
    union i2c_smbus_data data;

    if (i2c_smbus_access (fd, I2C_SMBUS_READ, reg, I2C_SMBUS_WORD_DATA, &data))
        return -1 ;
    else
        return data.word & 0xFFFF ;
}

//------------------------------------------------------------------------------
int i2c_write (int fd, int data)
{
    return i2c_smbus_access (fd, I2C_SMBUS_WRITE, data, I2C_SMBUS_BYTE, NULL) ;
}

//------------------------------------------------------------------------------
int i2c_write_byte (int fd, int reg, int value)
{
    union i2c_smbus_data data ;

    data.byte = value ;
    return i2c_smbus_access (fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_BYTE_DATA, &data) ;
}

//------------------------------------------------------------------------------
int i2c_write_word (int fd, int reg, int value)
{
    union i2c_smbus_data data ;

    data.word = value ;
    return i2c_smbus_access (fd, I2C_SMBUS_WRITE, reg, I2C_SMBUS_WORD_DATA, &data) ;
}

//------------------------------------------------------------------------------
int i2c_close (int fd)
{
    if (fd && (I2C_Mode == eI2C_MODE_HW))
        close (fd);

    /* gpio i2c slave address clear */
    I2C_SLAVE_ADDR = 0;

    return 0;
}

//------------------------------------------------------------------------------
int i2c_open (const char *device_info)
{
    I2C_Mode = check_i2c_mode (device_info);

    switch (I2C_Mode) {
        case eI2C_MODE_HW:      return i2c_open_hw   (device_info);
        case eI2C_MODE_GPIO:    return i2c_open_gpio (device_info);
        default :               return -1;
    }
}

//------------------------------------------------------------------------------
int i2c_open_device (const char *device_info, int device_addr)
{
    int fd;

    if ((fd = i2c_open (device_info)) < 0)
        return -1;

    if (i2c_set_addr (fd, device_addr)) {
        i2c_close (fd);
        return -1;
    }
    return fd;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


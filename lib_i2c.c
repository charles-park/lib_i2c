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
static int  check_i2c_mode      (const char *device_info);

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
//------------------------------------------------------------------------------
static int check_i2c_mode (const char *device_info)
{
    char str[5];

    memset (str, 0, sizeof(str));
    memcpy (str, device_info, sizeof(str)-1);

    if (!strncmp ("gpio", str, sizeof(str)-1))
        return eI2C_MODE_GPIO;
    if (!strncmp ("/dev", str, sizeof(str)-1))
        return eI2C_MODE_HW;

    return -1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int i2c_set_addr (int fd, int device_addr)
{
    if (IS_GPIO_I2C(fd)) {
        return gpio_i2c_saddr(fd, device_addr);
    } else {
        if (ioctl (fd, I2C_SLAVE, device_addr) < 0) {
            fprintf (stderr, "Can't setup device : device adddr is 0x%02x\n", device_addr);
            return -1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
int i2c_smbus_access (int fd, char rw, uint8_t command, int size, union i2c_smbus_data *data)
{
    struct i2c_smbus_ioctl_data args ;

    args.read_write = rw ;
    args.command    = command ;
    args.size       = size ;
    args.data       = data ;

    if (IS_GPIO_I2C(fd))    return gpio_i2c_ctrl (fd, &args);
    else                    return ioctl (fd, I2C_SMBUS, &args) ;
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
//------------------------------------------------------------------------------
int i2c_read_block   (int fd, int command, int size, uint8_t *data)
{
    int cnt;

    if (IS_GPIO_I2C(fd)) {
        struct i2c_smbus_ioctl_data args ;

        args.read_write = I2C_SMBUS_READ ;
        args.command    = command ;
        args.size       = size ;
        args.data       = (union i2c_smbus_data *)data ;
        cnt = gpio_i2c_ctrl (fd, &args);
    } else {
        uint8_t reg = command;

        // command or reg write
        if (write (fd, (void *)&reg, 1) != 1) {
            fprintf (stderr, "%s : reg = 0x%02x write error\n", __func__, command);
            return 0;
        }
    }
    if (size != 0)  cnt = read (fd, data, size);

    return cnt;
}

//------------------------------------------------------------------------------
int i2c_write_block  (int fd, int command, int size, uint8_t *data)
{
    int cnt;

    if (IS_GPIO_I2C(fd)) {
        struct i2c_smbus_ioctl_data args ;

        args.read_write = I2C_SMBUS_WRITE ;
        args.command    = command ;
        args.size       = size ;
        args.data       = (union i2c_smbus_data *)data ;
        cnt = gpio_i2c_ctrl (fd, &args);
    } else {
        uint8_t *pbuf = malloc (size +1);

        if (pbuf != NULL)   pbuf [0] = command;
        if (size != 0)      memcpy (&pbuf[1], data, size);

        cnt = write (fd, (void *)pbuf, (size + 1));

        if (pbuf != NULL)   free (pbuf);
    }

    return cnt;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int i2c_close (int fd)
{
    if (!IS_GPIO_I2C(fd)) {
        close (fd);
    }

    /* gpio i2c slave address clear */
//    I2C_SLAVE_ADDR = 0;

    return 0;
}

//------------------------------------------------------------------------------
static int i2c_open_hw (const char *device_info)
{
    int fd;
    if ((fd = open (device_info, O_RDWR)) < 0) {
        fprintf (stderr, "%s : Unable to open I2C device : %s\n", __func__, device_info);
        return -1;
    }
    return fd;
}

//------------------------------------------------------------------------------
int i2c_open (const char *device_info)
{
    return (check_i2c_mode (device_info) == eI2C_MODE_GPIO) ?
        gpio_i2c_open (device_info) : i2c_open_hw (device_info);
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


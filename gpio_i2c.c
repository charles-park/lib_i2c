//------------------------------------------------------------------------------
/**
 * @file gpio_i2c.c
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
#define	GPIO_CONTROL_PATH   "/sys/class/gpio"
#define GPIO_SET_DELAY      50
#define	GPIO_DIR_OUT        1
#define	GPIO_DIR_IN         0

#define I2C_READ_FLAG       0x01

enum {  LOW = 0, HIGH = 1, };

//------------------------------------------------------------------------------
// function prototype
//------------------------------------------------------------------------------
static int      gpio_export     (int gpio);
static int      gpio_direction  (int gpio, int status);
static int      gpio_set_value  (int gpio, int s_value);
static int      gpio_get_value  (int gpio, int *g_value);
static int      gpio_unexport   (int gpio);
static void     gpio_i2c_start  (int restart);
static void     gpio_i2c_stop   (void);
static int      i2c_write_bits  (uint8_t wd);
static int      i2c_read_bits   (void);

static int gpio_i2c_write (struct i2c_smbus_ioctl_data *args);
static int gpio_i2c_read  (struct i2c_smbus_ioctl_data *args);

//------------------------------------------------------------------------------
int     gpio_i2c_init   (int scl_gpio, int sda_gpio);
void    gpio_i2c_close  (void);
int     gpio_i2c_ctrl   (struct i2c_smbus_ioctl_data *args);

int GPIO_I2C_SDA = 0, GPIO_I2C_SCL = 0;

//------------------------------------------------------------------------------
static void udelay (int delay)
{
    usleep (delay);
}

//------------------------------------------------------------------------------
static int gpio_export (int gpio)
{
    char fname[256];
    FILE *fp;

    memset (fname, 0x00, sizeof(fname));
    sprintf (fname, "%s/export", GPIO_CONTROL_PATH);
    if ((fp = fopen (fname, "w")) != NULL) {
        char gpio_num[4];
        memset (gpio_num, 0x00, sizeof(gpio_num));
        sprintf (gpio_num, "%d", gpio);
        fwrite (gpio_num, strlen(gpio_num), 1, fp);
        fclose (fp);
        return 1;
    }
    printf ("%s error : gpio = %d\n", __func__, gpio);
    return 0;
}

//------------------------------------------------------------------------------
static int gpio_direction (int gpio, int status)
{
    char fname[256];
    FILE *fp;

    memset (fname, 0x00, sizeof(fname));
    sprintf (fname, "%s/gpio%d/direction", GPIO_CONTROL_PATH, gpio);
    if ((fp = fopen (fname, "w")) != NULL) {
        char gpio_status[4];
        memset (gpio_status, 0x00, sizeof(gpio_status));
        sprintf(gpio_status, "%s", status ? "out" : "in");
        fwrite (gpio_status, strlen(gpio_status), 1, fp);
        fclose (fp);
        return 1;
    }
    printf ("%s error : gpio = %d\n", __func__, gpio);
    return 0;
}

//------------------------------------------------------------------------------
static int gpio_set_value (int gpio, int s_value)
{
    char fname[256];
    FILE *fp;

    memset (fname, 0x00, sizeof(fname));
    sprintf (fname, "%s/gpio%d/value", GPIO_CONTROL_PATH, gpio);
    if ((fp = fopen (fname, "w")) != NULL) {
        fputc (s_value ? '1' : '0', fp);
        fclose (fp);
        return 1;
    }
    printf ("%s error : gpio = %d\n", __func__, gpio);
    return 0;
}

//------------------------------------------------------------------------------
static int gpio_get_value (int gpio, int *g_value)
{
    char fname[256];
    FILE *fp;

    memset (fname, 0x00, sizeof(fname));
    sprintf (fname, "%s/gpio%d/value", GPIO_CONTROL_PATH, gpio);
    if ((fp = fopen (fname, "r")) != NULL) {
        *g_value = (fgetc (fp) - '0');
        fclose (fp);
        return 1;
    }
    printf ("%s error : gpio = %d\n", __func__, gpio);
    return 0;
}

//------------------------------------------------------------------------------
static int gpio_unexport (int gpio)
{
    char fname[256];
    FILE *fp;

    memset (fname, 0x00, sizeof(fname));
    sprintf (fname, "%s/unexport", GPIO_CONTROL_PATH);
    if ((fp = fopen (fname, "w")) != NULL) {
        char gpio_num[4];
        memset (gpio_num, 0x00, sizeof(gpio_num));
        sprintf (gpio_num, "%d", gpio);
        fwrite (gpio_num, strlen(gpio_num), 1, fp);
        fclose (fp);
        return 1;
    }
    printf ("%s error : gpio = %d\n", __func__, gpio);
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void gpio_i2c_start     (int restart)
{
    if (!GPIO_I2C_SDA || !GPIO_I2C_SCL)     return;

    gpio_set_value (GPIO_I2C_SDA, LOW); udelay(GPIO_SET_DELAY);
    gpio_set_value (GPIO_I2C_SCL, LOW); udelay(GPIO_SET_DELAY);
    if (restart) {
        gpio_set_value (GPIO_I2C_SDA, HIGH);    udelay(GPIO_SET_DELAY);
        gpio_set_value (GPIO_I2C_SCL, HIGH);    udelay(GPIO_SET_DELAY);
        gpio_set_value (GPIO_I2C_SDA, LOW);     udelay(GPIO_SET_DELAY);
        gpio_set_value (GPIO_I2C_SCL, LOW);     udelay(GPIO_SET_DELAY);
    }
}

/*---------------------------------------------------------------------------*/
static void gpio_i2c_stop      (void)
{
    gpio_set_value (GPIO_I2C_SCL, HIGH);    udelay(GPIO_SET_DELAY);
    gpio_set_value (GPIO_I2C_SDA, HIGH);    udelay(GPIO_SET_DELAY);
}

/*---------------------------------------------------------------------------*/
static int i2c_write_bits   (uint8_t wd)
{
    int i;

    for (i = 0; i < 8; i++) {
        gpio_set_value (GPIO_I2C_SDA, (wd & 0x80) ? HIGH : LOW);
        wd <<= 1;
        gpio_set_value (GPIO_I2C_SCL, HIGH);    udelay(GPIO_SET_DELAY);
        gpio_set_value (GPIO_I2C_SCL, LOW);     udelay(GPIO_SET_DELAY);
    }
    // ack check
    gpio_set_value (GPIO_I2C_SCL, HIGH);        udelay(GPIO_SET_DELAY);
    gpio_direction (GPIO_I2C_SDA, GPIO_DIR_IN); udelay(GPIO_SET_DELAY);
    gpio_get_value (GPIO_I2C_SDA, &i);
    gpio_direction (GPIO_I2C_SDA, GPIO_DIR_OUT);udelay(GPIO_SET_DELAY);
    gpio_set_value (GPIO_I2C_SCL, LOW);         udelay(GPIO_SET_DELAY);

    return i;
}

/*---------------------------------------------------------------------------*/
static int i2c_read_bits   (void)
{
    int i, rd, rb;

    gpio_direction (GPIO_I2C_SDA, GPIO_DIR_IN);
    for (i = 0, rd = 0, rb = 0; i < 8; i++) {
        gpio_set_value (GPIO_I2C_SCL, HIGH);    udelay(GPIO_SET_DELAY);
        rd <<= 1;
        gpio_get_value (GPIO_I2C_SDA, &rb);
        rd |= rb ? 1 : 0;
        gpio_set_value (GPIO_I2C_SCL, LOW);     udelay(GPIO_SET_DELAY);
    }
    gpio_direction (GPIO_I2C_SDA, GPIO_DIR_OUT);

    return rd;
}

/*---------------------------------------------------------------------------*/
static int gpio_i2c_write (struct i2c_smbus_ioctl_data *args)
{
    uint16_t i = 0;
    union i2c_smbus_data *pdata = args->data;

   gpio_i2c_start (0);
    if (i2c_write_bits (I2C_SLAVE_ADDR))    goto wr_out;
    if (args->size == 0)    {   i = 1;      goto wr_out;    }
    if (i2c_write_bits (args->command))     goto wr_out;

    for (i = 0; i < args->size; i++)
        if (i2c_write_bits (pdata->block[i]))   goto wr_out;

wr_out:
    gpio_i2c_stop  ();

#if defined (_DEBUG_GPIO_I2C_)
    if (i != size) {
        printf ("%s(error) : addr = 0x%02X, reg = 0x%02X, size = %d\r\n", addr, reg, size);
    }
#endif
    return i;
}

//------------------------------------------------------------------------------
static int gpio_i2c_read  (struct i2c_smbus_ioctl_data *args)
{
    uint16_t i = 0;
    union i2c_smbus_data *pdata = args->data;

    gpio_i2c_start (0);
    if (i2c_write_bits (I2C_SLAVE_ADDR))    goto rd_out;
    if (args->size == 0)    {   i = 1;      goto rd_out;    }
    if (i2c_write_bits (args->command))     goto rd_out;

    // Read
    gpio_i2c_start (1);
    if (i2c_write_bits (I2C_SLAVE_ADDR | I2C_READ_FLAG))    goto rd_out;

    for (i = 0; i < args->size; i++) {
        pdata->block[i] = i2c_read_bits ();
        // ack send except last byte.
        if (i < (args->size -1)) {
            gpio_set_value (GPIO_I2C_SDA, LOW);     udelay(GPIO_SET_DELAY);
            gpio_set_value (GPIO_I2C_SCL, HIGH);    udelay(GPIO_SET_DELAY);
            gpio_set_value (GPIO_I2C_SCL, LOW);     udelay(GPIO_SET_DELAY);
            gpio_set_value (GPIO_I2C_SDA, HIGH);    udelay(GPIO_SET_DELAY);
        }
    }
rd_out:
    gpio_i2c_stop  ();

#if defined (_DEBUG_GPIO_I2C_)
    if (i != size) {
        printf ("%s(error) : addr = 0x%02X, reg = 0x%02X, size = %d\r\n", addr, reg, size);
    }
#endif
    return i;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int gpio_i2c_init (int scl_gpio, int sda_gpio)
{
    if (!gpio_export (scl_gpio))    return -1;
    if (!gpio_export (sda_gpio))    return -1;

    GPIO_I2C_SCL = scl_gpio;
    GPIO_I2C_SDA = sda_gpio;
    gpio_direction (GPIO_I2C_SCL, GPIO_DIR_OUT);
    gpio_direction (GPIO_I2C_SDA, GPIO_DIR_OUT);

    return FD_GPIO_I2C;
}

//------------------------------------------------------------------------------
void gpio_i2c_close (void)
{
    if (GPIO_I2C_SCL)   gpio_unexport (GPIO_I2C_SCL);
    if (GPIO_I2C_SDA)   gpio_unexport (GPIO_I2C_SDA);
    I2C_SLAVE_ADDR = 0;
}

//------------------------------------------------------------------------------
int gpio_i2c_ctrl (struct i2c_smbus_ioctl_data *args)
{
    int ret = 0;

    if (!I2C_SLAVE_ADDR || (I2C_Mode != eI2C_MODE_GPIO))
        return -1;

    switch (args->size) {
        case I2C_SMBUS_BYTE:        args->size = 0;     break;
        case I2C_SMBUS_BYTE_DATA:   args->size = 1;     break;
        case I2C_SMBUS_WORD_DATA:   args->size = 2;     break;
        default :
            return -1;
    }

    ret = args->read_write ? gpio_i2c_read (args) : gpio_i2c_write (args);

    return ret ? 0 : -1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

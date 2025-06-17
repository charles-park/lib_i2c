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

#include <pthread.h>

#include "lib_i2c.h"
#include "gpio_i2c.h"

//------------------------------------------------------------------------------
#define	GPIO_CONTROL_PATH   "/sys/class/gpio"
#define GPIO_SET_DELAY      50
#define	GPIO_DIR_OUT        1
#define	GPIO_DIR_IN         0

#define I2C_READ_FLAG       0x01

//------------------------------------------------------------------------------
enum {  LOW = 0, HIGH = 1, };

//------------------------------------------------------------------------------
volatile unsigned char GPIO_I2C_SLOT = 0;

#define I2C_SLOT_MASK   0x01
#define I2C_SLOT_MAX    8

struct gpio_i2c_info {
    int fd;
    char saddr;
    int sda;
    int scl;
};

pthread_mutex_t mutex_gpio_i2c = PTHREAD_MUTEX_INITIALIZER;

struct gpio_i2c_info    InfoGPIOI2C[I2C_SLOT_MAX];

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// function prototype
//------------------------------------------------------------------------------

static void udelay          (int delay);
static int  gpio_export     (int gpio);
static int  gpio_direction  (int gpio, int status);
static int  gpio_set_value  (int gpio, int s_value);
static int  gpio_get_value  (int gpio, int *g_value);
static int  gpio_unexport   (int gpio);
static void gpio_i2c_start  (int restart);
static void gpio_i2c_stop   (void);

static int  i2c_write_bits  (uint8_t wd);
static int  i2c_read_bits   (void);
static int  i2c_set_gpio    (int fd);

static int  gpio_i2c_write  (int fd, struct i2c_smbus_ioctl_data *args);
static int  gpio_i2c_read   (int fd, struct i2c_smbus_ioctl_data *args);
static int  find_i2c_slot   (void);
static int  gpio_i2c_init   (int scl_gpio, int sda_gpio);

//------------------------------------------------------------------------------
void    gpio_i2c_close  (int fd);
int     gpio_i2c_saddr  (int fd, int device_addr);
int     gpio_i2c_ctrl   (int fd, struct i2c_smbus_ioctl_data *args);
int     gpio_i2c_open   (const char *device_info);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int GPIO_I2C_SDA = 0, GPIO_I2C_SCL = 0, GPIO_I2C_SADDR = 0;

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
/*---------------------------------------------------------------------------*/
static int i2c_set_gpio (int fd)
{
    int i;
    for (i = 0; i < I2C_SLOT_MAX; i++) {
        if (fd == InfoGPIOI2C[i].fd) {
            GPIO_I2C_SCL   = InfoGPIOI2C[i].scl;
            GPIO_I2C_SDA   = InfoGPIOI2C[i].sda;
            GPIO_I2C_SADDR = InfoGPIOI2C[i].saddr << 1;
            return 1;
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
static int gpio_i2c_write (int fd, struct i2c_smbus_ioctl_data *args)
{
    uint16_t i = 0;
    union i2c_smbus_data *pdata = args->data;

    // Setup I2C GPIO & Slave Addr
    if (!i2c_set_gpio(fd))  return 0;

    // Mutex on
    pthread_mutex_lock(&mutex_gpio_i2c);
    gpio_i2c_stop  ();

    gpio_i2c_start (0);
    if (i2c_write_bits (GPIO_I2C_SADDR))    goto wr_out;
    if (args->size == 0)    {   i = 1;      goto wr_out;    }
    if (i2c_write_bits (args->command))     goto wr_out;

    for (i = 0; i < args->size; i++)
        if (i2c_write_bits (pdata->block[i]))   goto wr_out;

wr_out:
    gpio_i2c_stop  ();

    // Mutex off
    pthread_mutex_unlock(&mutex_gpio_i2c);

#if defined (_DEBUG_GPIO_I2C_)
printf ("%s : addr = 0x%02X, reg = 0x%02X, size = %d\r\n", __func__, GPIO_I2C_SADDR, args->command, args->size);
printf ("%s : data = 0x%02X, i = %d\r\n", __func__, pdata->block[0], i);

if (i != args->size) {
        printf ("%s(error) : addr = 0x%02X, reg = 0x%02X, size = %d\r\n", addr, reg, size);
    }
#endif
    return i;
}

//------------------------------------------------------------------------------
static int gpio_i2c_read  (int fd, struct i2c_smbus_ioctl_data *args)
{
    uint16_t i = 0;
    union i2c_smbus_data *pdata = args->data;

    // Setup I2C GPIO & Slave Addr
    if (!i2c_set_gpio(fd))  return 0;

    // Mutex on
    pthread_mutex_lock(&mutex_gpio_i2c);
    gpio_i2c_stop  ();

    gpio_i2c_start (0);
    if (i2c_write_bits (GPIO_I2C_SADDR))    goto rd_out;
    if (args->size == 0)    {   i = 1;      goto rd_out;    }
    if (i2c_write_bits (args->command))     goto rd_out;

    // Read
    gpio_i2c_start (1);
    if (i2c_write_bits (GPIO_I2C_SADDR | I2C_READ_FLAG))    goto rd_out;

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

    // Mutex off
    pthread_mutex_unlock(&mutex_gpio_i2c);
#if defined (_DEBUG_GPIO_I2C_)
    if (i != size) {
        printf ("%s(error) : addr = 0x%02X, reg = 0x%02X, size = %d\r\n", addr, reg, size);
    }
#endif
    return i;
}

//------------------------------------------------------------------------------
static int find_i2c_slot (void)
{
    int i;
    for (i = 0; i < I2C_SLOT_MAX; i++) {

        if (!(GPIO_I2C_SLOT & (I2C_SLOT_MASK << i))) {
            GPIO_I2C_SLOT |= (I2C_SLOT_MASK) << i;
            return i;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
static int gpio_i2c_init (int scl_gpio, int sda_gpio)
{
    int slot_num = 0;

    if (!gpio_export (scl_gpio))    return -1;
    if (!gpio_export (sda_gpio))    return -1;

    if ((slot_num = find_i2c_slot()) != -1) {
        InfoGPIOI2C[slot_num].fd    = (slot_num | GPIO_I2C_FLAG);
        InfoGPIOI2C[slot_num].scl   = scl_gpio;
        InfoGPIOI2C[slot_num].sda   = sda_gpio;
        InfoGPIOI2C[slot_num].saddr = -1;
    }
    else    return -1;

    gpio_direction (InfoGPIOI2C[slot_num].scl, GPIO_DIR_OUT);
    gpio_direction (InfoGPIOI2C[slot_num].sda, GPIO_DIR_OUT);

    i2c_set_gpio(InfoGPIOI2C[slot_num].fd);

    gpio_i2c_stop  ();

    return  InfoGPIOI2C[slot_num].fd;
}

//------------------------------------------------------------------------------
void gpio_i2c_close (int fd)
{
    int i;
    GPIO_I2C_SDA = 0, GPIO_I2C_SCL = 0, GPIO_I2C_SADDR = 0;

    for (i = 0; i < I2C_SLOT_MAX; i++) {
        if (InfoGPIOI2C[i].fd == fd) {
            InfoGPIOI2C[i].fd   = 0;
            InfoGPIOI2C[i].scl  = 0;
            InfoGPIOI2C[i].sda  = 0;
            InfoGPIOI2C[i].saddr= 0;
            // Slot empty
            GPIO_I2C_SLOT &= ~(I2C_SLOT_MASK << i);
        }
    }
}

//------------------------------------------------------------------------------
int gpio_i2c_saddr (int fd, int device_addr)
{
    int i;
    for (i = 0; i < I2C_SLOT_MAX; i++) {
        if (fd == InfoGPIOI2C[i].fd)    {
            InfoGPIOI2C[i].saddr = device_addr;
            return 0;
        }
    }
    return -1;
}

//------------------------------------------------------------------------------
int gpio_i2c_ctrl (int fd, struct i2c_smbus_ioctl_data *args)
{
    int ret = 0;

    if (!IS_GPIO_I2C(fd))   return -1;

    switch (args->size) {
        case I2C_SMBUS_BYTE:        args->size  = 0;    break;
        case I2C_SMBUS_BYTE_DATA:   args->size  = 1;    break;
        case I2C_SMBUS_WORD_DATA:   args->size  = 2;    break;
//        default:                    args->size -= 1;    break;
        default:                    break;
}

    ret = args->read_write ? gpio_i2c_read (fd, args) : gpio_i2c_write (fd, args);

    return ret ? 0 : -1;
}

//------------------------------------------------------------------------------
int gpio_i2c_open (const char *device_info)
{
    char gpio_info [64], *p;
    int scl_gpio, sda_gpio, i;

    memset (gpio_info, 0, sizeof(gpio_info));
    memcpy (gpio_info, device_info, strlen (device_info));

    if ((p = strtok (gpio_info, ",")) != NULL) {
        if (strncmp (p, "gpio", sizeof("gpio")))   return -1;

        for (i = 0, scl_gpio = 0, sda_gpio = 0; i < 2; i++ ) {
            p = strtok (NULL, ",");
            if (!strncmp (p, "scl", sizeof("scl"))) {
                p = strtok (NULL, ",");
                scl_gpio = atoi (p);
            }
            else if (!strncmp (p, "sda", sizeof("sda"))) {
                p = strtok (NULL, ",");
                sda_gpio = atoi (p);
            }
        }
        if (!scl_gpio || !sda_gpio)     return -1;
    }
    return gpio_i2c_init (scl_gpio, sda_gpio);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

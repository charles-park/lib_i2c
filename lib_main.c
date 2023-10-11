//------------------------------------------------------------------------------
/**
 * @file lib_main.c
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
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <getopt.h>

#include "lib_i2c.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if defined(__LIB_I2C_APP__)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void print_usage (const char *prog)
{
    puts("");
    printf("Usage: %s [-D:device] [-b] [-w]\n", prog);
    puts("\n"
         "  -D --Device         Control Device node\n"
         "  -b --byte_read      byte_read func used\n"
         "  -w --word_read      word_read func used\n"
         "\n"
         "  e.g) find i2c device from i2c-node\n"
         "       lib_i2c -D /dev/i2c-0\n"
    );
    exit(1);
}

//------------------------------------------------------------------------------
/* Control server variable */
//------------------------------------------------------------------------------
static char *OPT_DEVICE_NODE    = NULL;
static int   OPT_MODE = 0;

//------------------------------------------------------------------------------
// 문자열 변경 함수. 입력 포인터는 반드시 메모리가 할당되어진 변수여야 함.
//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = toupper(*p);
}

//------------------------------------------------------------------------------
static void parse_opts (int argc, char *argv[])
{
    while (1) {
        static const struct option lopts[] = {
            { "Device",     0, 0, 'D' },
            { "read_word",  0, 0, 'w' },
            { "read_byte",  0, 0, 'b' },
            { NULL, 0, 0, 0 },
        };
        int c;

        c = getopt_long(argc, argv, "D:wbh", lopts, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'D':
            OPT_DEVICE_NODE = optarg;
            break;
        /* detect word read */
        case 'w':
            OPT_MODE = 1;
            break;
        /* detect byte read */
        case 'b':
            OPT_MODE = 2;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            break;
        }
    }
}

//------------------------------------------------------------------------------
int detect_i2c (int fd)
{
    int i, cnt, ret;

    switch (OPT_MODE) {
        default:
        case 0: printf ("%s : i2c_read func used.\n", __func__);        break;
        case 1: printf ("%s : i2c_read_word func used.\n", __func__);   break;
        case 2: printf ("%s : i2c_read_byte func used.\n", __func__);   break;
    }

    for (i = I2C_ADDR_START, cnt = 0; i < I2C_ADDR_END; i++) {
        i2c_set_addr(fd, i);
        switch (OPT_MODE) {
            case 1:
                ret = i2c_read_word (fd, 0);
                break;
            case 2:
                ret = i2c_read_byte (fd, 0);
                break;
            case 0:
            default:
                ret = i2c_read (fd);
                break;
        }
        if(ret != -1) {
            printf ("I2C ack detect %s (Device Addr : 0x%02x)\n",
                OPT_DEVICE_NODE, i);
            cnt ++;
        }
    }
    if (!cnt)
        printf ("I2C Device not found!\n");

    return cnt ? 0 : 1;
}

//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
int main (int argc, char *argv[])
{
    int fd;

    parse_opts(argc, argv);

    if (OPT_DEVICE_NODE == NULL)
        print_usage(argv[0]);

    if ((fd = i2c_open(OPT_DEVICE_NODE)) < 0)
        return -1;

    detect_i2c (fd);
    close(fd);

    return 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif  // #if defined(__LIB_I2C_APP__)
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

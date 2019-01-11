#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/platform-defs.h>
#include <ddk/protocol/i2c.h>
#include <ddk/protocol/i2c-lib.h>


#include <zircon/types.h>
#include <zircon/syscalls.h>

// FIDL include
// TODO

#define LCD_CMD         0x80
#define WRITE_CMD       0x40
#define DISPLAY_SIZE    34  // We have 2x 16 positions at the LCD. To provide enough space for one '\n' a line, the buffer is set to 34.  

typedef struct {
    zx_device_t* device;
    mtx_t lock;
    i2c_protocol_t i2c;
    char display_text[DISPLAY_SIZE];
} grove_lcd_t;

typedef struct {
    uint8_t cmd;
    uint8_t val;
} i2c_cmd_t;


static void grove_lcd_release(void* ctx) {
    zxlogf(INFO, "%s\n", __func__);
    
    grove_lcd_t* grove_lcd = ctx;
    free(grove_lcd);
}

static zx_protocol_device_t grove_lcd_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_lcd_release,
    //TODO
};

static int grove_lcd_init_thread(void* arg) {
    zxlogf(INFO, "%s\n", __func__);

    zx_status_t status;
    grove_lcd_t* grove_lcd = arg;

    mtx_lock(&grove_lcd->lock);

    i2c_cmd_t setup_cmds[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x02},
        {LCD_CMD, 0x08 | 0x04},
        {LCD_CMD, 0x28},
        {WRITE_CMD, 'I'},
        {WRITE_CMD, 'n'},
        {WRITE_CMD, 'i'},
        {WRITE_CMD, 't'},
    };

    for (int i = 0; i < (int) (sizeof(setup_cmds) / sizeof(*setup_cmds)); i++) {
        status = i2c_write_sync(&grove_lcd->i2c, &setup_cmds[i].cmd, sizeof(setup_cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-lcd: write failed\n");
            goto init_failed;
        }
    }

    mtx_unlock(&grove_lcd->lock);

    device_make_visible(grove_lcd->device);

    zxlogf(INFO, "grove-lcd: init thread runned successfully!\n");

    return ZX_OK;

init_failed:
    zxlogf(ERROR, "grove-lcd: init thread failed\n");
    mtx_unlock(&grove_lcd->lock);
    device_remove(grove_lcd->device);
    free(grove_lcd);
    return ZX_ERR_IO;
}

static zx_status_t grove_lcd_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;

    grove_lcd_t* grove_lcd = calloc(1, sizeof(*grove_lcd));
    if (!grove_lcd) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_I2C, &grove_lcd->i2c) != ZX_OK) {
        free(grove_lcd);
        zxlogf(ERROR, "Failed to fetch the I2C protocol for the grove lcd driver - not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "grove-lcd-drv",
        .ctx = grove_lcd,
        .ops = &grove_lcd_device_protocol,
        .flags = DEVICE_ADD_INVISIBLE,
    };

    if ((status = device_add(parent, &args, &grove_lcd->device)) != ZX_OK) {
        free(grove_lcd);
        return status;
    }

    thrd_t thrd;
    int thrd_ret = thrd_create_with_name(&thrd, grove_lcd_init_thread, grove_lcd, "grove_lcd_init_thread");
    if (thrd_ret != thrd_success) {
        status = thrd_ret;
        device_remove(grove_lcd->device);
        free(grove_lcd);
    }

    zxlogf(INFO, "grove-lcd driver bind runned successfully\n");

    return status;
}

static zx_driver_ops_t grove_lcd_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = grove_lcd_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(grove-lcd-drv, grove_lcd_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PDEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_LCD),
ZIRCON_DRIVER_END(grove-lcd-drv)
// clang-format on

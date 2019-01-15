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
#include <ddk/protocol/i2c-lib.h>
#include <ddk/protocol/i2c.h>
#include <ddk/protocol/platform/device.h>

#include <zircon/syscalls.h>
#include <zircon/types.h>

// FIDL include
#include <zircon/display/grove/rgb/c/fidl.h>

#define RED 0x04
#define GREEN 0x03
#define BLUE 0x02

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

typedef struct {
    zx_device_t* device;
    pdev_protocol_t pdev;
    mtx_t lock;
    i2c_protocol_t i2c_rgb;
    i2c_protocol_t i2c_lcd;
    color_t color;
} grove_t;

typedef struct {
    uint8_t cmd;
    uint8_t val;
} i2c_cmd_t;

static void grove_release(void* ctx) {
    zxlogf(INFO, "%s\n", __func__);
    grove_t* grove = ctx;
    free(grove);
}

static zx_status_t grove_read(void* ctx, void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);
    if (off == 0) {
        grove_t* grove = ctx;
        char tmp[50];
        *actual = snprintf(tmp, sizeof(tmp), "Grove LCD RGB Status:\nRed: %x\nGreen: %x\nBlue: %x\n",
                           grove->color.red, grove->color.green, grove->color.blue);
        if (*actual > count) {
            *actual = count;
        }
        memcpy(buf, tmp, *actual);
    } else {
        *actual = 0;
    }
    return ZX_OK;
}

static zx_status_t grove_write(void* ctx, const void* buf, size_t count, zx_off_t off, size_t* actual) {
    // at the moment just defined for RGB
    zxlogf(INFO, "%s\n", __func__);
    zxlogf(INFO, "grove write needs the following format to work: r g b\ne.g. r0 g255 b0\n Values are allowed between 0 and 255\n");

    zx_status_t status;
    grove_t* grove = ctx;

    char delim[] = " ";
    int i = 0;

    char tmp_str[count];
    memcpy(tmp_str, buf, count);

    *actual = count;

    mtx_lock(&grove->lock);

    char* ptr = strtok(tmp_str, delim);
    while (ptr != NULL) {
        zxlogf(INFO, "grove read token %s\n", ptr);
        if (i == 0 && ptr[0] == 'r') {
            grove->color.red = atoi(++ptr);
        } else if (i == 1 && ptr[0] == 'g') {
            grove->color.green = atoi(++ptr);
        } else if (i == 2 && ptr[0] == 'b') {
            grove->color.blue = atoi(++ptr);
        } else {
            zxlogf(ERROR, "wrong input format!\n");
            status = ZX_OK;
            goto fail;
        }
        ptr = strtok(NULL, delim);
        i++;
    }

    i2c_cmd_t cmds[] = {
        {RED, grove->color.red},
        {GREEN, grove->color.green},
        {BLUE, grove->color.blue},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        status = i2c_write_sync(&grove->i2c_rgb, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write failed\n");
            goto fail;
        }
    }

fail:
    mtx_unlock(&grove->lock);

    return status;
}

static zx_status_t grove_fidl_set_color(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    grove_t* grove = ctx;
    zx_status_t status;

    mtx_lock(&grove->lock);

    grove->color.red = red;
    grove->color.green = green;
    grove->color.blue = blue;

    i2c_cmd_t cmds[] = {
        {RED, grove->color.red},
        {GREEN, grove->color.green},
        {BLUE, grove->color.blue},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        status = i2c_write_sync(&grove->i2c_rgb, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write failed\n");
            mtx_unlock(&grove->lock);
            return status;
        }
    }

    mtx_unlock(&grove->lock);

    return status;
}

static zx_status_t grove_fidl_get_color(void* ctx, fidl_txn_t* txn) {
    grove_t* grove = ctx;

    zx_status_t status = zircon_display_grove_rgb_RgbGetColor_reply(txn, grove->color.red, grove->color.green, grove->color.blue);
    return status;
}

static zircon_display_grove_rgb_Rgb_ops_t fidl_ops = {
    .SetColor = grove_fidl_set_color,
    .GetColor = grove_fidl_get_color,
};

static zx_status_t grove_fidl_message(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    zx_status_t status = zircon_display_grove_rgb_Rgb_dispatch(ctx, txn, msg, &fidl_ops);
    return status;
}

static zx_protocol_device_t grove_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_release,
    .read = grove_read,
    .write = grove_write,
    .message = grove_fidl_message,
};

static int grove_init_thread(void* arg) {
    zxlogf(INFO, "%s\n", __func__);
    grove_t* grove = arg;
    zx_status_t status;

    mtx_lock(&grove->lock);

    // init RGB
    grove->color.green = 0xff;

    i2c_cmd_t cmds[] = {
        {0x00, 0x00},
        {0x01, 0x00},
        {0x08, 0xaa},
        {RED, grove->color.red},
        {GREEN, grove->color.green},
        {BLUE, grove->color.blue},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        zxlogf(INFO, "grove: command: %x %x\n", cmds[i].cmd, cmds[i].val);
        status = i2c_write_sync(&grove->i2c_rgb, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write failed\n");
            goto init_failed;
        }
    }

    // init LCD
    // TODO

    mtx_unlock(&grove->lock);

    zxlogf(INFO, "making device visible\n");
    device_make_visible(grove->device);

    zxlogf(INFO, "grove driver init thread runned successfully!\n");

    return ZX_OK;

init_failed:
    zxlogf(ERROR, "grove init thread failed\n");
    mtx_unlock(&grove->lock);
    device_remove(grove->device);
    free(grove);
    return ZX_ERR_IO;
}

static zx_status_t grove_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;

    // allocate memory for context data structure (grove_t)
    grove_t* grove = calloc(1, sizeof(*grove));
    if (!grove) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_PDEV, &grove->pdev) != ZX_OK) {
        free(grove);
        zxlogf(ERROR, "Failed to fetch the PDEV protocol for the grove lcd rgb driver - not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    size_t actual;
    if (pdev_get_protocol(&grove->pdev, ZX_PROTOCOL_I2C, 0, &grove->i2c_rgb, sizeof(grove->i2c_rgb), &actual) != ZX_OK) {
        free(grove);
        zxlogf(ERROR, "Failed to fetch the I2C protocol (rgb) for the grove lcd rgb driver - not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (pdev_get_protocol(&grove->pdev, ZX_PROTOCOL_I2C, 1, &grove->i2c_lcd, sizeof(grove->i2c_lcd), &actual) != ZX_OK) {
        free(grove);
        zxlogf(ERROR, "Failed to fetch the I2C protocol (lcd) for the grove lcd rgb driver - not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "grove-drv",
        .ctx = grove,
        .ops = &grove_device_protocol,
        .flags = DEVICE_ADD_INVISIBLE,
    };

    if ((status = device_add(parent, &args, &grove->device)) != ZX_OK) {
        free(grove);
        return status;
    }

    thrd_t thrd;
    int thrd_ret = thrd_create_with_name(&thrd, grove_init_thread, grove, "grove_init_thread");
    if (thrd_ret != thrd_success) {
        status = thrd_ret;
        device_remove(grove->device);
        free(grove);
    }

    zxlogf(INFO, "grove driver bind runned successfully\n");

    return status;
}

static zx_driver_ops_t grove_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = grove_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(grove-drv, grove_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PDEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_PDEV),
ZIRCON_DRIVER_END(grove-drv)
// clang-format on

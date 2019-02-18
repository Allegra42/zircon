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
    mtx_t lock;
    i2c_protocol_t i2c;
    color_t color;
} grove_rgb_t;

typedef struct {
    uint8_t cmd;
    uint8_t val;
} i2c_cmd_t;

static void grove_rgb_release(void* ctx) {
    zxlogf(INFO, "%s\n", __func__);
    grove_rgb_t* grove_rgb = ctx;
    free(grove_rgb);
}

static zx_status_t grove_rgb_read(void* ctx, void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);
    if (off == 0) {
        grove_rgb_t* grove_rgb = ctx;
        char tmp[50];
        *actual = snprintf(tmp, sizeof(tmp), "Grove LCD RGB Status:\nRed: %x\nGreen: %x\nBlue: %x\n",
                           grove_rgb->color.red, grove_rgb->color.green, grove_rgb->color.blue);
        if (*actual > count) {
            *actual = count;
        }
        memcpy(buf, tmp, *actual);
    } else {
        *actual = 0;
    }
    return ZX_OK;
}

static zx_status_t grove_rgb_write(void* ctx, const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);
    zxlogf(INFO, "grove-rgb write needs the following format to work: r g b\ne.g. r0 g255 b0\n Values are allowed between 0 and 255\n");

    zx_status_t status;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    grove_rgb_t* grove_rgb = ctx;

    char delim[] = " ";
    int i = 0;

    char tmp_str[count];
    memcpy(tmp_str, buf, count);

    *actual = count;

    mtx_lock(&grove_rgb->lock);

    char* ptr = strtok(tmp_str, delim);
    while (ptr != NULL) {
        zxlogf(INFO, "grove-rgb read token %s\n", ptr);
        if (i == 0 && ptr[0] == 'r') {
            red = atoi(++ptr);
        } else if (i == 1 && ptr[0] == 'g') {
            green = atoi(++ptr);
        } else if (i == 2 && ptr[0] == 'b') {
            blue = atoi(++ptr);
        } else {
            zxlogf(ERROR, "wrong input format!\n");
            status = ZX_OK;
            goto fail;
        }
        ptr = strtok(NULL, delim);
        i++;
    }

    i2c_cmd_t cmds[] = {
        {RED, red},
        {GREEN, green},
        {BLUE, blue},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        status = i2c_write_sync(&grove_rgb->i2c, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-rgb: write failed\n");
            goto fail;
        }
    }
    grove_rgb->color.red = red;
    grove_rgb->color.green = green;
    grove_rgb->color.blue = blue;

fail:
    mtx_unlock(&grove_rgb->lock);
    return status;
}

static zx_status_t grove_rgb_fidl_set_color(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    grove_rgb_t* grove_rgb = ctx;
    zx_status_t status;

    mtx_lock(&grove_rgb->lock);

    i2c_cmd_t cmds[] = {
        {RED, red},
        {GREEN, green},
        {BLUE, blue},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        status = i2c_write_sync(&grove_rgb->i2c, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-rgb: write failed\n");
            mtx_unlock(&grove_rgb->lock);
            return status;
        }
    }
    grove_rgb->color.red = red;
    grove_rgb->color.green = green;
    grove_rgb->color.blue = blue;

    mtx_unlock(&grove_rgb->lock);

    return status;
}

static zx_status_t grove_rgb_fidl_get_color(void* ctx, fidl_txn_t* txn) {
    grove_rgb_t* grove_rgb = ctx;

    zx_status_t status = zircon_display_grove_rgb_RgbGetColor_reply(txn, grove_rgb->color.red, grove_rgb->color.green, grove_rgb->color.blue);
    return status;
}

static zircon_display_grove_rgb_Rgb_ops_t fidl_ops = {
    .SetColor = grove_rgb_fidl_set_color,
    .GetColor = grove_rgb_fidl_get_color,
};

static zx_status_t grove_rgb_fidl_message(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    zx_status_t status = zircon_display_grove_rgb_Rgb_dispatch(ctx, txn, msg, &fidl_ops);
    return status;
}

static zx_protocol_device_t grove_rgb_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_rgb_release,
    .read = grove_rgb_read,
    .write = grove_rgb_write,
    .message = grove_rgb_fidl_message,
};

static int grove_rgb_init_thread(void* arg) {
    zxlogf(INFO, "%s\n", __func__);
    grove_rgb_t* grove_rgb = arg;
    uint8_t green;
    zx_status_t status;

    mtx_lock(&grove_rgb->lock);

    green = 0xff;

    i2c_cmd_t cmds[] = {
        {0x00, 0x00},
        {0x01, 0x00},
        {0x08, 0xaa},
        {RED, 0x00},
        {GREEN, green},
        {BLUE, 0x00},
    };

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(*cmds)); i++) {
        status = i2c_write_sync(&grove_rgb->i2c, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-rgb: write failed\n");
            goto init_failed;
        }
    }
    grove_rgb->color.green = green;

    mtx_unlock(&grove_rgb->lock);

    zxlogf(INFO, "making device visible\n");
    device_make_visible(grove_rgb->device);

    zxlogf(INFO, "grove-rgb driver init thread runned successfully!\n");

    return ZX_OK;

init_failed:
    zxlogf(ERROR, "grove-rgb init thread failed\n");
    mtx_unlock(&grove_rgb->lock);
    device_remove(grove_rgb->device);
    free(grove_rgb);
    return ZX_ERR_IO;
}

static zx_status_t grove_rgb_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;

    // allocate memory for context data structure (grove_rgb_t)
    grove_rgb_t* grove_rgb = calloc(1, sizeof(*grove_rgb));
    if (!grove_rgb) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_I2C, &grove_rgb->i2c) != ZX_OK) {
        free(grove_rgb);
        zxlogf(ERROR, "Failed to fetch the I2C protocol for the grove lcd rgb driver - not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "grove-rgb-drv",
        .ctx = grove_rgb,
        .ops = &grove_rgb_device_protocol,
        .flags = DEVICE_ADD_INVISIBLE,
    };

    if ((status = device_add(parent, &args, &grove_rgb->device)) != ZX_OK) {
        free(grove_rgb);
        return status;
    }

    thrd_t thrd;
    int thrd_ret = thrd_create_with_name(&thrd, grove_rgb_init_thread, grove_rgb, "grove_rgb_init_thread");
    if (thrd_ret != thrd_success) {
        status = thrd_ret;
        device_remove(grove_rgb->device);
        free(grove_rgb);
    }

    zxlogf(INFO, "grove-rgb driver bind runned successfully\n");

    return status;
}

static zx_driver_ops_t grove_rgb_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = grove_rgb_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(grove-rgb-drv, grove_rgb_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_RGB),
ZIRCON_DRIVER_END(grove-rgb-drv)
// clang-format on

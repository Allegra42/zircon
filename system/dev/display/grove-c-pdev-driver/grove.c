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
#include <zircon/display/grove/pdev/c/fidl.h>

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))

#define RED 0x04
#define GREEN 0x03
#define BLUE 0x02
#define LCD_CMD 0x80
#define LINE_SIZE 17

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
    char line_one[LINE_SIZE + 1];
    char line_two[LINE_SIZE + 1];
} grove_t;

typedef struct {
    uint8_t cmd;
    uint8_t val;
} i2c_cmd_t;


zx_status_t grove_write_line(void* ctx, i2c_cmd_t* cmds, int cmd_elements, char* raw_line) {
    zx_status_t status;
    grove_t* grove = ctx;

    char line[LINE_SIZE + 1] = {" "};
    snprintf(line, (strlen(raw_line) + 2 <= LINE_SIZE ? strlen(raw_line) + 2 : LINE_SIZE + 1), "@%s", raw_line);
    
    mtx_lock(&grove->lock);

    for (int i = 0; i < cmd_elements; i++) {
        status = i2c_write_sync(&grove->i2c_lcd, &cmds[i].cmd, sizeof(cmds[i]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write to i2c device failed\n");
            goto error;
        }
    }

    status = i2c_write_sync(&grove->i2c_lcd, line, strlen(line));
    if (status != ZX_OK) {
        zxlogf(ERROR, "grove: write to i2c device failed\n");
    }

error:
    mtx_unlock(&grove->lock);
    return status;
}

static void grove_release(void* ctx) {
    zxlogf(INFO, "%s\n", __func__);
    grove_t* grove = ctx;
    free(grove);
}

static zx_status_t grove_fidl_set_color(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    grove_t* grove = ctx;
    zx_status_t status;

    mtx_lock(&grove->lock);

    i2c_cmd_t cmds[] = {
        {RED, red},
        {GREEN, green},
        {BLUE, blue},
    };

    for (int i = 0; i < (int)ARRAY_SIZE(cmds); i++) {
        status = i2c_write_sync(&grove->i2c_rgb, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write to i2c device failed\n");
            mtx_unlock(&grove->lock);
            return status;
        }
    }
    
    grove->color.red = red;
    grove->color.green = green;
    grove->color.blue = blue;

    mtx_unlock(&grove->lock);

    return status;
}

static zx_status_t grove_fidl_get_color(void* ctx, fidl_txn_t* txn) {
    grove_t* grove = ctx;

    zx_status_t status = zircon_display_grove_pdev_PdevGetColor_reply(txn, grove->color.red, grove->color.green, grove->color.blue);
    return status;
}

static zx_status_t grove_fidl_clear_lcd(void* ctx) {
    zx_status_t status;
    grove_t* grove = ctx;
    i2c_cmd_t cmd = {LCD_CMD, 0x01};

    mtx_lock(&grove->lock);
    status = i2c_write_sync(&grove->i2c_lcd, &cmd, sizeof(cmd));
    if (status != ZX_OK) {
        zxlogf(ERROR, "grove: write to i2c device failed\n");
        goto fail;
    }
    
    snprintf(grove->line_one, sizeof(grove->line_one), " ");
    snprintf(grove->line_two, sizeof(grove->line_one), " ");

fail:
    mtx_unlock(&grove->lock);
    return status;
}

static zx_status_t grove_fidl_write_first_line(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    grove_t* grove = ctx;

    uint8_t val = (position | 0x80);
    i2c_cmd_t cmds[] = {
        {LCD_CMD, val}, // set cursor
    };

    status = grove_write_line((void*)grove, cmds, ARRAY_SIZE(cmds), (char*)line_data);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s failed with status code %d\n", __func__, status);
        return status;
    }
    snprintf(grove->line_one, sizeof(grove->line_one), "%s", line_data);

    return status;
}

static zx_status_t grove_fidl_write_second_line(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    grove_t* grove = ctx;

    uint8_t val = (position | 0xc0);
    i2c_cmd_t cmds[] = {
        {LCD_CMD, val}, // set cursor
    };

    status = grove_write_line((void*)grove, cmds, ARRAY_SIZE(cmds), (char*)line_data);
     if (status != ZX_OK) {
        zxlogf(ERROR, "%s failed with status code %d\n", __func__, status);
        return status;
    }
    snprintf(grove->line_two, sizeof(grove->line_two), "%s", line_data);

    return status;
}

static zx_status_t grove_fidl_read_lcd(void* ctx, fidl_txn_t* txn) {
    zx_status_t status;
    grove_t* grove = ctx;

    char tmp[LINE_SIZE * 2];
    snprintf(tmp, sizeof(tmp), "%s\n%s", grove->line_one, grove->line_two);

    status = zircon_display_grove_pdev_PdevReadLcd_reply(txn, tmp, strlen(tmp));
    return status;
}

static zx_status_t grove_fidl_get_line_size(void* ctx, fidl_txn_t* txn) {
    return zircon_display_grove_pdev_PdevGetLineSize_reply(txn, LINE_SIZE - 1);
}

static zircon_display_grove_pdev_Pdev_ops_t fidl_ops = {
    .SetColor = grove_fidl_set_color,
    .GetColor = grove_fidl_get_color,
    .ClearLcd = grove_fidl_clear_lcd,
    .WriteFirstLine = grove_fidl_write_first_line,
    .WriteSecondLine = grove_fidl_write_second_line,
    .ReadLcd = grove_fidl_read_lcd,
    .GetLineSize = grove_fidl_get_line_size,
};

static zx_status_t grove_fidl_message(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    zx_status_t status = zircon_display_grove_pdev_Pdev_dispatch(ctx, txn, msg, &fidl_ops);
    return status;
}

static zx_protocol_device_t grove_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_release,
    .message = grove_fidl_message,
};

static int grove_init_thread(void* arg) {
    zxlogf(INFO, "%s\n", __func__);
    grove_t* grove = arg;
    zx_status_t status;
    uint8_t green;

    mtx_lock(&grove->lock);

    // init RGB
    green = 0xff;

    i2c_cmd_t cmds[] = {
        {0x00, 0x00},
        {0x01, 0x00},
        {0x08, 0xaa},
        {RED, 0x00},
        {GREEN, green},
        {BLUE, 0x00},
    };

    for (int i = 0; i < (int)ARRAY_SIZE(cmds); i++) {
        status = i2c_write_sync(&grove->i2c_rgb, &cmds[i].cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove: write failed\n");
            goto init_failed;
        }
    }
    grove->color.green = green;

    // init LCD
    i2c_cmd_t setup_cmds[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x02},
        {LCD_CMD, 0x0c},
        {LCD_CMD, 0x28},
    };

    for (int i = 0; i < (int)ARRAY_SIZE(cmds); i++) {
        status = i2c_write_sync(&grove->i2c_lcd, &setup_cmds[i].cmd, sizeof(setup_cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-lcd: write to i2c device failed\n");
            goto init_failed;
        }
    }

    status = i2c_write_sync(&grove->i2c_lcd, "@Initialized", 12);
    if (status != ZX_OK) {
        zxlogf(ERROR, "grove-lcd: write to i2c device failed\n");
        goto init_failed;
    }

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
        .name = "grove-pdev-drv",
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

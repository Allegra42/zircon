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
#include <zircon/display/grove/lcd/c/fidl.h>

#define LCD_CMD 0x80
#define WRITE_CMD 0x40
#define LINE_SIZE 17

typedef struct {
    zx_device_t* device;
    mtx_t lock;
    i2c_protocol_t i2c;
    char line_one[LINE_SIZE + 1];
    char line_two[LINE_SIZE + 1];
} grove_lcd_t;

typedef struct {
    uint8_t cmd;
    uint8_t val;
} i2c_cmd_t;

void grove_lcd_prepare_text(char* input, char* line) {
    char tmp[LINE_SIZE];

    snprintf(tmp, (strlen(input) + 1 < LINE_SIZE ? strlen(input) + 1 : LINE_SIZE), input);
    strcpy(line, "@");
    strcat(line, tmp);
}

zx_status_t grove_lcd_write_line(void* ctx, i2c_cmd_t* cmds, char* raw_line) {
    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;

    char line[LINE_SIZE + 1] = {" "};
    grove_lcd_prepare_text(raw_line, line);

    mtx_lock(&grove_lcd->lock);

    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(&cmds)); i++) {
        status = i2c_write_sync(&grove_lcd->i2c, &cmds[i].cmd, sizeof(cmds[i]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-lcd: write failed\n");
            goto error;
        }
    }

    status = i2c_write_sync(&grove_lcd->i2c, line, strlen(line));

error:
    mtx_unlock(&grove_lcd->lock);
    return status;
}

zx_status_t grove_lcd_fidl_clear(void* ctx) {
    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;
    i2c_cmd_t cmd = {LCD_CMD, 0x01};

    mtx_lock(&grove_lcd->lock);
    status = i2c_write_sync(&grove_lcd->i2c, &cmd, sizeof(cmd));
    mtx_unlock(&grove_lcd->lock);

    return status;
}

zx_status_t grove_lcd_fidl_write_first_line(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;

    snprintf(grove_lcd->line_one, sizeof(grove_lcd->line_one), line_data);

    uint8_t val = (position | 0x80);
    i2c_cmd_t cmds[] = {
        {LCD_CMD, val}, // set cursor
    };

    status = grove_lcd_write_line((void*)grove_lcd, cmds, grove_lcd->line_one);

    return status;
}

zx_status_t grove_lcd_fidl_write_second_line(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;

    snprintf(grove_lcd->line_two, sizeof(grove_lcd->line_two), line_data);

    uint8_t val = (0xc0);
    i2c_cmd_t cmds[] = {
        {LCD_CMD, val}, // set cursor
    };

    status = grove_lcd_write_line((void*)grove_lcd, cmds, grove_lcd->line_two);

    return status;
}

zx_status_t grove_lcd_fidl_read_lcd(void* ctx, fidl_txn_t* txn) {
    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;

    char tmp[LINE_SIZE * 2];
    strcpy(tmp, grove_lcd->line_one);
    strcat(tmp, "\n");
    strcat(tmp, grove_lcd->line_two);

    status = zircon_display_grove_lcd_LcdReadLcd_reply(txn, tmp, strlen(tmp));
    return status;
}

zx_status_t grove_lcd_fidl_get_line_size(void* ctx, fidl_txn_t* txn) {
    zx_status_t status;
    uint8_t linesize = LINE_SIZE - 1;

    status = zircon_display_grove_lcd_LcdGetLineSize_reply(txn, linesize);
    return status;
}

static void grove_lcd_release(void* ctx) {
    zxlogf(INFO, "%s\n", __func__);

    grove_lcd_t* grove_lcd = ctx;
    free(grove_lcd);
}

static zx_status_t grove_lcd_read(void* ctx, void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);
    if (off == 0) {
        grove_lcd_t* grove_lcd = ctx;

        char tmp[60];
        *actual = snprintf(tmp, sizeof(tmp), "Grove LCD RGB Content:\n%s\n%s\n",
                           grove_lcd->line_one, grove_lcd->line_two);

        if (*actual > count) {
            *actual = count;
        }

        memcpy(buf, tmp, *actual);

    } else {
        *actual = 0;
    }
    return ZX_OK;
}

static zx_status_t grove_lcd_write(void* ctx, const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    zx_status_t status;
    grove_lcd_t* grove_lcd = ctx;

    zxlogf(INFO, "%s:  buf %s\n", __func__, (char*)buf);
    snprintf(grove_lcd->line_one, strlen(buf) < LINE_SIZE ? strlen(buf) : LINE_SIZE, buf);
    *actual = count;

    i2c_cmd_t cmds[] = {
        {LCD_CMD, 0x01}, // clear display
        {LCD_CMD, 0x02}, // return home
    };

    status = grove_lcd_write_line((void*)grove_lcd, cmds, grove_lcd->line_one);

    return status;
}

static zircon_display_grove_lcd_Lcd_ops_t grove_lcd_fidl_ops = {
    .Clear = grove_lcd_fidl_clear,
    .WriteFirstLine = grove_lcd_fidl_write_first_line,
    .WriteSecondLine = grove_lcd_fidl_write_second_line,
    .ReadLcd = grove_lcd_fidl_read_lcd,
    .GetLineSize = grove_lcd_fidl_get_line_size,
};

static zx_status_t grove_lcd_message(void* ctx, fidl_msg_t* msg, fidl_txn_t* txn) {
    return zircon_display_grove_lcd_Lcd_dispatch(ctx, txn, msg, &grove_lcd_fidl_ops);
}

static zx_protocol_device_t grove_lcd_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_lcd_release,
    .read = grove_lcd_read,
    .write = grove_lcd_write,
    .message = grove_lcd_message,
};

static int grove_lcd_init_thread(void* arg) {
    zxlogf(INFO, "%s\n", __func__);

    zx_status_t status;
    grove_lcd_t* grove_lcd = arg;

    mtx_lock(&grove_lcd->lock);

    i2c_cmd_t setup_cmds[] = {
        {LCD_CMD, 0x01}, // clear display
        {LCD_CMD, 0x02}, // return home (cursor)
        {LCD_CMD, 0x0c}, // display on, no cursor
        {LCD_CMD, 0x28}, // enable 2 lines
    };

    for (int i = 0; i < (int)(sizeof(setup_cmds) / sizeof(*setup_cmds)); i++) {
        status = i2c_write_sync(&grove_lcd->i2c, &setup_cmds[i].cmd, sizeof(setup_cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-lcd: write failed\n");
            goto init_failed;
        }
    }

    // That's a bit mad way to send a whole string at ones.
    // By decoding target register (0x40) and the actual string in an array,
    // you have to be aware of padding. Not funny.
    // BUT, 0x40 can easily be decoded as a char. It's "@".
    // As a string prefix, it is interpreted as the target register, just as intended.
    // The following chars are send as specified by the API, serialized on this address.
    status = i2c_write_sync(&grove_lcd->i2c, "@Init", 5);

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
ZIRCON_DRIVER_BEGIN(grove-lcd-drv, grove_lcd_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_LCD),
ZIRCON_DRIVER_END(grove-lcd-drv)
// clang-format on

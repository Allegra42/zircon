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

//TODO add FIDL include


typedef struct {
    zx_device_t* device;
    i2c_protocol_t i2c;
    /* i2c_channel_t channel; */
} grove_rgb_t;


static void i2c_complete(zx_status_t status, const uint8_t* data, size_t actual, void* cookie) {
    grove_rgb_t* dev = cookie;
    if (!dev) {
        if (status != ZX_OK) {
            zxlogf(ERROR, "i2c transaction failed\n");
        }
        return;
    }
}

static void grove_rgb_release(void* ctx) {
    grove_rgb_t* grove_rgb = ctx;
    /* i2c_channel_release(&grove_rgb->channel); */
    free(grove_rgb);
}


static zx_protocol_device_t groce_rgb_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = grove_rgb_release,
};


static zx_status_t grove_rgb_bind(void* ctx, zx_device_t* parent) {
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

    // is the channel needed? TODO replace error handling with gotos
    /* zx_status_t status = i2c_get_channel(&grove_rgb->i2c, 0, &grove_rgb->channel);
     * if (status != ZX_OK) {
     *     free(grove_rgb);
     *     zxlogf(ERROR, "Failed to get channel: %d\n", (int)status);
     *     return status;
     * } */

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "grove-lcd-rgb-drv",
        .ctx = grove_rgb,
        .ops = &groce_rgb_device_protocol,
    };

    if ((status = device_add(parent, &args, &grove_rgb->device)) != ZX_OK) {
        free(grove_rgb);
        return status;
    }
    
    //TODO more specific device initialization


    /* uint8_t buf[] = {0x08, 0xAA};
     * if ((status = i2c_write_read_sync(&grove_rgb->i2c, buf, sizeof(buf), NULL, 0)) != ZX_OK) {
     *     return status;
     * } */

   
    uint8_t buf[2];
    buf[0] = 0;
    buf[1] = 0;
    do {
    status = i2c_write_sync(&grove_rgb->i2c, buf, 2);
    } while (status != ZX_OK);
    if (status != ZX_OK) {
        zxlogf(ERROR, "Could not write on grove rgb i2c, status %d\n", status);
    }
    
    buf[0] = 1;
    buf[1] = 0;
    do {
    status = i2c_write_sync(&grove_rgb->i2c, buf, 2);
    } while (status != ZX_OK);
    if (status != ZX_OK) {
        zxlogf(ERROR, "Could not write on grove rgb i2c, status %d\n", status);
    }
    
    buf[0] = 0x08;
    buf[1] = 0xaa;
    do {
    status = i2c_write_sync(&grove_rgb->i2c, buf, 2);
    } while (status != ZX_OK);
    if (status != ZX_OK) {
        zxlogf(ERROR, "Could not write on grove rgb i2c, status %d\n", status);
    }
    
    buf[0] = 3;
    buf[1] = 0xff;
    do {
    status = i2c_write_sync(&grove_rgb->i2c, buf, 2);
    } while (status != ZX_OK);
    if (status != ZX_OK) {
        zxlogf(ERROR, "Could not write on grove rgb i2c, status %d\n", status);
    }
    
    
    zxlogf(INFO, "grove-rgb driver bind runned successfully!\n");

    return status;
}

static zx_driver_ops_t grove_rgb_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = grove_rgb_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(grove-lcd-rgb-drv, grove_rgb_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PDEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_LCD),
ZIRCON_DRIVER_END(grove-lcd-rgb-drv)
// clang-format on

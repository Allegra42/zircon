#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/debug.h>
#include <ddk/platform-defs.h>
#include <ddk/protocol/i2c-lib.h>

#include <zircon/syscalls.h>

#include <fbl/algorithm.h>
#include <fbl/auto_call.h>
#include <fbl/auto_lock.h>
#include <fbl/string.h>
#include <fbl/string_printf.h>
#include <fbl/unique_ptr.h>

#include "grove-rgb.h"
#include <zircon/display/grove/rgb/c/fidl.h>

namespace grove {

zx_status_t GroveRgbDevice::DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    if (off == 0) {
        fbl::String tmp = fbl::StringPrintf("Grove LCD RGB Status:\nRed: 0x%x\nGreen: 0x%x\nBlue: 0x%0x\n", color_red, color_green, color_blue);
        *actual = tmp.length();
        if (*actual > count) {
            *actual = count;
        }
        memcpy(buf, tmp.c_str(), *actual);
    } else {
        *actual = 0;
    }
    return ZX_OK;
}

zx_status_t GroveRgbDevice::DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    zx_status_t status;
    char delim[] = " ";
    char tmp_str[count];
    int i = 0;

    fbl::AutoLock lock(&i2c_lock);

    *actual = count;
    memcpy(tmp_str, buf, count);

    char* ptr = strtok(tmp_str, delim);
    while (ptr != NULL) {
        if (i == 0 && ptr[0] == 'r') {
            color_red = static_cast<uint8_t>(atoi(++ptr));
        } else if (i == 1 && ptr[0] == 'g') {
            color_green = static_cast<uint8_t>(atoi(++ptr));
        } else if (i == 2 && ptr[0] == 'b') {
            color_blue = static_cast<uint8_t>(atoi(++ptr));
        } else {
            zxlogf(ERROR, "wrong input format!\n");
            return ZX_OK;
        }
        ptr = strtok(NULL, delim);
        i++;
    }

    I2cCmd cmd[] = {
        {RED, color_red},
        {GREEN, color_green},
        {BLUE, color_blue},
    };
    for (const auto& i : cmd) {
        status = i2c_rgb.WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }

    return ZX_OK;
}

zx_status_t GroveRgbDevice::SetColor(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    zx_status_t status;
    auto& self = *static_cast<GroveRgbDevice*>(ctx);
    fbl::AutoLock lock(&self.i2c_lock);

    self.color_red = red;
    self.color_green = green;
    self.color_blue = blue;

    I2cCmd cmd[] = {
        {RED, self.color_red},
        {GREEN, self.color_green},
        {BLUE, self.color_blue},
    };
    for (const auto& i : cmd) {
        status = self.i2c_rgb.WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }
    return ZX_OK;
}

zx_status_t GroveRgbDevice::GetColor(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveRgbDevice*>(ctx);
    return zircon_display_grove_rgb_RgbGetColor_reply(txn, self.color_red, self.color_green, self.color_blue);
}

zircon_display_grove_rgb_Rgb_ops_t fidl_rgb_ops = {
    .SetColor = GroveRgbDevice::SetColor,
    .GetColor = GroveRgbDevice::GetColor,
};

zx_status_t GroveRgbDevice::DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn) {
    return zircon_display_grove_rgb_Rgb_dispatch(this, txn, msg, &fidl_rgb_ops);
}

zx_status_t GroveRgbDevice::RgbInit() {
    zx_status_t status;

    if (!i2c_rgb.is_valid()) {
        return ZX_ERR_NO_RESOURCES;
    }

    color_green = 0xff;

    I2cCmd cmd[] = {
        {0, 0},
        {1, 0},
        {0x08, 0xaa},
        {RED, color_red},
        {GREEN, color_green},
        {BLUE, color_blue},
    };
    fbl::AutoLock lock(&i2c_lock);
    for (const auto& i : cmd) {
        status = i2c_rgb.WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    DdkMakeVisible();
    return status;

error:
    return ZX_ERR_IO;
}

zx_status_t GroveRgbDevice::Bind(zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    fbl::AllocChecker ac;

    auto dev = fbl::make_unique_checked<grove::GroveRgbDevice>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    auto status = dev->DdkAdd("grove-rgb-drv", DEVICE_ADD_INVISIBLE);
    status = dev->RgbInit();

    __UNUSED auto ptr = dev.release();

    return status;
}

void GroveRgbDevice::DdkRelease() {
    zxlogf(INFO, "%s\n", __func__);
    delete this;
}

} // namespace grove

extern "C" zx_status_t grove_rgb_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    return grove::GroveRgbDevice::Bind(parent);
}

#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/debug.h>
#include <ddk/platform-defs.h>
#include <ddk/protocol/i2c-lib.h>
#include <ddk/protocol/i2c.h>

#include <zircon/syscalls.h>

#include <fbl/algorithm.h>
#include <fbl/auto_call.h>
#include <fbl/auto_lock.h>
#include <fbl/unique_ptr.h>

#include <string>

#include "grove.h"
#include <zircon/display/grove/rgb/c/fidl.h>

namespace grove {

zx_status_t GroveDevice::DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    //TODO better, more c++ like impl?
    if (off == 0) {
        char tmp[50];
        *actual = snprintf(tmp, sizeof(tmp), "Grove LCD RGB Status:\nRed: %x\nGreen: %x\nBlue: %x\n", color_red, color_green, color_blue);
        if (*actual > count) {
            *actual = count;
        }
        memcpy(buf, tmp, *actual);
    } else {
        *actual = 0;
    }
    return ZX_OK;
}

zx_status_t GroveDevice::DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    //TODO better, more c++ like impl?
    zx_status_t status;
    char delim[] = " ";
    char tmp_str[count];
    int i = 0;

    fbl::AutoLock lock(&i2c_lock);

    *actual = count;
    memcpy(tmp_str, buf, count);

    char* ptr = strtok(tmp_str, delim);
    while (ptr != NULL) {
        zxlogf(INFO, "grove-rgb read token %s\n", ptr);
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
        status = i2c_rgb->WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }

    return ZX_OK;
}

zx_status_t GroveDevice::SetColor(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    zx_status_t status;
    auto& self = *static_cast<GroveDevice*>(ctx);
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
        status = self.i2c_rgb->WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }
    return ZX_OK;
}

zx_status_t GroveDevice::GetColor(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveDevice*>(ctx);
    return zircon_display_grove_rgb_RgbGetColor_reply(txn, self.color_red, self.color_green, self.color_blue);
}

zircon_display_grove_rgb_Rgb_ops_t fidl_ops = {
    .SetColor = GroveDevice::SetColor,
    .GetColor = GroveDevice::GetColor,
};

zx_status_t GroveDevice::DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn) {
    zxlogf(INFO, "%s\n", __func__);
    return zircon_display_grove_rgb_Rgb_dispatch(this, txn, msg, &fidl_ops);
}

zx_status_t GroveDevice::PdevInit() {
    if (!pdev.is_valid()) {
        return ZX_ERR_NO_RESOURCES;
    }

    i2c_rgb = pdev.GetI2c(0);
    if (!i2c_rgb) {
        zxlogf(INFO, "grove: i2c_rgb is not available\n");
    } else {
        this->RgbInit();
    }

    i2c_lcd = pdev.GetI2c(1);
    if (!i2c_lcd) {
        zxlogf(INFO, "grove: i2c_lcd is not available\n");
    } else {
        this->LcdInit();
    }

    if (!i2c_rgb && !i2c_lcd) {
        zxlogf(ERROR, "grove: none i2c device is available\n");
        return ZX_ERR_NO_RESOURCES;
    }

    return ZX_OK;
}

zx_status_t GroveDevice::RgbInit() {
    zx_status_t status;

    // no need for checking if I2C device is valid in this case,
    // the method is just called if the device is available

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
        status = i2c_rgb->WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "rgb-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }
    return status;
}

zx_status_t GroveDevice::LcdInit() {
    zx_status_t status = ZX_OK;

    // no need for checking if I2C device is valid in this case,
    // the method is just called if the device is available

    //TODO init LCD

    return status;
}

zx_status_t GroveDevice::Bind(zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    fbl::AllocChecker ac;

    auto dev = fbl::make_unique_checked<grove::GroveDevice>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    auto status = dev->PdevInit();

    status = dev->DdkAdd("grove-cpp-pdev-drv");

    __UNUSED auto ptr = dev.release();

    return status;
}

void GroveDevice::DdkRelease() {
    zxlogf(INFO, "%s\n", __func__);
    delete this;
}

} // namespace grove

extern "C" zx_status_t grove_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    return grove::GroveDevice::Bind(parent);
}

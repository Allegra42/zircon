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
#include <fbl/unique_ptr.h>

#include <string>

#include "grove.h"
#include <zircon/display/grove/pdev/c/fidl.h>

namespace grove {

zx_status_t GroveDevice::WriteLine(I2cCmd* cmds, int cmd_elements, fbl::String raw_line) {
    zx_status_t status;

    fbl::String tmp = fbl::String::Concat({"@", raw_line});

    fbl::AutoLock lock(&this->i2c_lock);
    for (int i = 0; i < cmd_elements; i++) {
        status = i2c_lcd->WriteSync(&cmds[i].cmd, sizeof(cmds[i]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "pdev-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }
    status = i2c_lcd->WriteSync(reinterpret_cast<const uint8_t*>(tmp.c_str()), static_cast<uint8_t>(tmp.length()));
    if (status != ZX_OK) {
        zxlogf(ERROR, "pdev-cpp: i2c.WriteSync failed\n");
        goto error;
    }

error:
    return status;
}

zx_status_t GroveDevice::ClearLcd(void* ctx) {
    zx_status_t status;
    auto& self = *static_cast<GroveDevice*>(ctx);
    I2cCmd cmd = {LCD_CMD, 0x01};

    fbl::AutoLock lock(&self.i2c_lock);
    status = self.i2c_lcd->WriteSync(&cmd.cmd, sizeof(cmd));
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s failed with status code %d\n", __func__, status);
        return status;
    }

    self.line_one = " ";
    self.line_two = " ";
    
    return status;
}

zx_status_t GroveDevice::WriteFirstLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    auto& self = *static_cast<GroveDevice*>(ctx);

    fbl::String data(line_data);

    uint8_t val = (position | 0x80);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    status = self.WriteLine(cmds, (int)(sizeof(cmds) / sizeof(*cmds)), data);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s failed with status code %d\n", __func__, status);
        return status;
    }

    self.line_one = data;
    
    return status;
}

zx_status_t GroveDevice::WriteSecondLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    zx_status_t status;
    auto& self = *static_cast<GroveDevice*>(ctx);

    fbl::String data(line_data);

    uint8_t val = (position | 0xc0);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    status = self.WriteLine(cmds, (int)(sizeof(cmds) / sizeof(*cmds)), data);
    if (status != ZX_OK) {
        zxlogf(ERROR, "%s failed with status code %d\n", __func__, status);
        return status;
    }

    self.line_two = data;

    return status;
}

zx_status_t GroveDevice::ReadLcd(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveDevice*>(ctx);

    fbl::String tmp = fbl::String::Concat({self.line_one, "\n", self.line_two});

    return zircon_display_grove_pdev_PdevReadLcd_reply(txn, tmp.c_str(), static_cast<size_t>(tmp.length()));
}

zx_status_t GroveDevice::GetLineSize(void* ctx, fidl_txn_t* txn) {
    return zircon_display_grove_pdev_PdevGetLineSize_reply(txn, 16);
}

zx_status_t GroveDevice::SetColor(void* ctx, uint8_t red, uint8_t green, uint8_t blue) {
    zx_status_t status;
    auto& self = *static_cast<GroveDevice*>(ctx);

    fbl::AutoLock lock(&self.i2c_lock);

    I2cCmd cmds[] = {
        {RED, red},
        {GREEN, green},
        {BLUE, blue},
    };
    for (const auto& i : cmds) {
        status = self.i2c_rgb->WriteSync(&i.cmd, sizeof(cmds[0]));

        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-pdev-cpp: i2c.WriteSync failed\n");
            return status;
        }
    }
   
    self.color_red = red;
    self.color_green = green;
    self.color_blue = blue;

    return ZX_OK;
}

zx_status_t GroveDevice::GetColor(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveDevice*>(ctx);
    return zircon_display_grove_pdev_PdevGetColor_reply(txn, self.color_red, self.color_green, self.color_blue);
}

zircon_display_grove_pdev_Pdev_ops_t fidl_pdev_ops = {
    .SetColor = GroveDevice::SetColor,
    .GetColor = GroveDevice::GetColor,
    .ClearLcd = GroveDevice::ClearLcd,
    .WriteFirstLine = GroveDevice::WriteFirstLine,
    .WriteSecondLine = GroveDevice::WriteSecondLine,
    .ReadLcd = GroveDevice::ReadLcd,
    .GetLineSize = GroveDevice::GetLineSize,
};

zx_status_t GroveDevice::DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn) {
    return zircon_display_grove_pdev_Pdev_dispatch(this, txn, msg, &fidl_pdev_ops);
}

zx_status_t GroveDevice::PdevInit() {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;

    if (!pdev.is_valid()) {
        goto error;
    }

    i2c_rgb = pdev.GetI2c(0);
    if (!i2c_rgb) {
        zxlogf(INFO, "grove-pdev-cpp: i2c_rgb is not available\n");
        goto error;
    }

    i2c_lcd = pdev.GetI2c(1);
    if (!i2c_lcd) {
        zxlogf(INFO, "grove-pdev-cpp: i2c_lcd is not available\n");
        goto error;
    }

    if ((status = RgbInit()) != ZX_OK) {
        goto error;
    }
    if ((status = LcdInit()) != ZX_OK) {
        goto error;
    }
    zxlogf(INFO, "grove-pdev-cpp: status %d\n", status);

    DdkMakeVisible();

    return status;

error:
    return ZX_ERR_NO_RESOURCES;
}

zx_status_t GroveDevice::RgbInit() {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;
    uint8_t green = 0xff;

    I2cCmd cmds[] = {
        {0x00, 0x00},
        {0x01, 0x00},
        {0x08, 0xaa},
        {RED, 0x00},
        {GREEN, green},
        {BLUE, 0x00},
    };
    fbl::AutoLock lock(&i2c_lock);
    for (const auto& i : cmds) {
        status = i2c_rgb->WriteSync(&i.cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-pdev-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }
    color_green = green;

    return status;

error:
    return ZX_ERR_IO;
}

zx_status_t GroveDevice::LcdInit() {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status = ZX_OK;
    fbl::String init("@Init");

    I2cCmd cmds[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x02},
        {LCD_CMD, 0x0c},
        {LCD_CMD, 0x28},
    };

    fbl::AutoLock lock(&i2c_lock);
    for (const auto& i : cmds) {
        status = i2c_lcd->WriteSync(&i.cmd, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "grove-pdev-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    status = i2c_lcd->WriteSync(reinterpret_cast<const uint8_t*>(init.c_str()), static_cast<uint8_t>(init.length()));
    if (status != ZX_OK) {
        goto error;
    }
    line_one = init;

    return status;

error:
    return ZX_ERR_IO;
}

zx_status_t GroveDevice::Bind(zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    fbl::AllocChecker ac;

    auto dev = fbl::make_unique_checked<grove::GroveDevice>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    auto status = dev->DdkAdd("grove-pdev-drv", DEVICE_ADD_INVISIBLE);
    status = dev->PdevInit();

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

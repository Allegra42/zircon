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
#include <fbl/unique_ptr.h>

#include "grove-lcd.h"
#include <zircon/display/grove/lcd/c/fidl.h>

namespace grove {

zx_status_t GroveLcdDevice::WriteLine(I2cCmd* cmds, fbl::String raw_line) {
    zx_status_t status;

    fbl::String tmp = fbl::String::Concat({"@", raw_line});

    fbl::AutoLock lock(&this->i2c_lock);
    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(&cmds)); i++) {
        status = this->i2c_lcd.WriteSync(&cmds[i].cmd, sizeof(cmds[i]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    status = i2c_lcd.WriteSync(reinterpret_cast<const uint8_t*>(tmp.c_str()), static_cast<uint8_t>(tmp.length()));
    if (status != ZX_OK) {
        zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
        goto error;
    }

error:
    return status;
}

zx_status_t GroveLcdDevice::DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    if (off == 0) {
        fbl::String tmp = fbl::String::Concat({"LCD Status:\n", this->line_one, "\n", this->line_two, "\n"});
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

zx_status_t GroveLcdDevice::DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    *actual = count;
    fbl::String tmp(reinterpret_cast<const char*>(buf), count - 1);

    I2cCmd cmds[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x80},
    };
    line_two = " ";

    return WriteLine(cmds, tmp);
}

zx_status_t GroveLcdDevice::ClearLcd(void* ctx) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);
    I2cCmd cmd = {LCD_CMD, 0x01};

    fbl::AutoLock lock(&self.i2c_lock);
    return self.i2c_lcd.WriteSync(&cmd.cmd, sizeof(cmd));
}

zx_status_t GroveLcdDevice::WriteFirstLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);

    fbl::String data(line_data);
    self.line_one = data;

    uint8_t val = (position | 0x80);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    return self.WriteLine(cmds, self.line_one);
}

zx_status_t GroveLcdDevice::WriteSecondLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);

    fbl::String data(line_data);
    self.line_two = data;

    uint8_t val = (position | 0xc0);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    return self.WriteLine(cmds, self.line_two);
}

zx_status_t GroveLcdDevice::ReadLcd(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);

    fbl::String tmp = fbl::String::Concat({self.line_one, "\n", self.line_two});

    return zircon_display_grove_lcd_LcdReadLcd_reply(txn, tmp.c_str(), static_cast<size_t>(tmp.length()));
}

zx_status_t GroveLcdDevice::GetLineSize(void* ctx, fidl_txn_t* txn) {
    return zircon_display_grove_lcd_LcdGetLineSize_reply(txn, LINE_SIZE - 1);
}

zircon_display_grove_lcd_Lcd_ops_t fidl_lcd_ops = {
    .ClearLcd = GroveLcdDevice::ClearLcd,
    .WriteFirstLine = GroveLcdDevice::WriteFirstLine,
    .WriteSecondLine = GroveLcdDevice::WriteSecondLine,
    .ReadLcd = GroveLcdDevice::ReadLcd,
    .GetLineSize = GroveLcdDevice::GetLineSize,
};

zx_status_t GroveLcdDevice::DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn) {
    return zircon_display_grove_lcd_Lcd_dispatch(this, txn, msg, &fidl_lcd_ops);
}

zx_status_t GroveLcdDevice::LcdInit() {
    zxlogf(INFO, "%s\n", __func__);
    zx_status_t status;
    fbl::String init("@Init");
    line_one = init;

    if (!this->i2c_lcd.is_valid()) {
        return ZX_ERR_NO_RESOURCES;
    }

    I2cCmd cmds[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x02},
        {LCD_CMD, 0x0c},
        {LCD_CMD, 0x28},
    };

    fbl::AutoLock lock(&i2c_lock);
    for (const auto& i : cmds) {
        status = i2c_lcd.WriteSync(&i.cmds, sizeof(cmds[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    status = i2c_lcd.WriteSync(reinterpret_cast<const uint8_t*>(line_one.c_str()), static_cast<uint8_t>(line_one.length()));
    if (status != ZX_OK) {
        zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
        goto error;
    }

    DdkMakeVisible();
    return status;

error:
    return ZX_ERR_IO;
}

zx_status_t GroveLcdDevice::Bind(zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    fbl::AllocChecker ac;

    auto dev = fbl::make_unique_checked<grove::GroveLcdDevice>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    auto status = dev->DdkAdd("grove-lcd-drv", DEVICE_ADD_INVISIBLE);
    status = dev->LcdInit();

    __UNUSED auto ptr = dev.release();

    return status;
}

void GroveLcdDevice::DdkRelease() {
    zxlogf(INFO, "%s\n", __func__);
    delete this;
}

} // namespace grove

extern "C" zx_status_t grove_lcd_bind(void* ctx, zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    return grove::GroveLcdDevice::Bind(parent);
}

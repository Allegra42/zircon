#include <threads.h>
#include <unistd.h>
#include <string.h>

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
#include <cstdio>

#include "grove-lcd.h"
#include <zircon/display/grove/lcd/c/fidl.h>

namespace grove {

zx_status_t GroveLcdDevice::WriteLine(I2cCmd* cmds, std::string raw_line) {
    zx_status_t status;

    raw_line.insert(0, "@");
    zxlogf(INFO, "%s raw_line is %s\n", __func__, raw_line.c_str());

    fbl::AutoLock lock(&i2c_lock);
    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(&cmds)); i++) {
        status = i2c_lcd.WriteSync(&cmds[i].cmd, sizeof(cmds[i]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    status = i2c_lcd.WriteSync(reinterpret_cast<const uint8_t*>(raw_line.c_str()), static_cast<uint8_t>(raw_line.length()));
    if (status != ZX_OK) {
        zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
        goto error;
    }

error:
    return status;
}

zx_status_t GroveLcdDevice::DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    //TODO better, more c++ like impl?
    return ZX_OK;
}

zx_status_t GroveLcdDevice::DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual) {
    zxlogf(INFO, "%s\n", __func__);

    //TODO better, more c++ like impl?

    return ZX_OK;
}

zx_status_t GroveLcdDevice::Clear(void* ctx) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);
    I2cCmd cmd = {LCD_CMD, 0x01};

    fbl::AutoLock lock(&self.i2c_lock);
    return self.i2c_lcd.WriteSync(&cmd.cmd, sizeof(cmd));
}

zx_status_t GroveLcdDevice::WriteFirstLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);

    std::string data(line_data);
    self.line_one.assign(data, LINE_SIZE < data.length() ? LINE_SIZE : data.length());

    uint8_t val = (position | 0x80);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    return self.WriteLine(cmds, self.line_one);
}

zx_status_t GroveLcdDevice::WriteSecondLine(void* ctx, uint8_t position, const char* line_data, size_t line_size) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);
    
    std::string data(line_data);
    self.line_two.assign(data, LINE_SIZE < data.length() ? LINE_SIZE : data.length());

    uint8_t val = (position | 0xc0);
    I2cCmd cmds[] = {
        {LCD_CMD, val},
    };

    return self.WriteLine(cmds, self.line_two);
}

zx_status_t GroveLcdDevice::ReadLcd(void* ctx, fidl_txn_t* txn) {
    auto& self = *static_cast<GroveLcdDevice*>(ctx);

    std::string tmp = self.line_one + self.line_two;

    return zircon_display_grove_lcd_LcdReadLcd_reply(txn, tmp.c_str(), static_cast<size_t>(tmp.length()));
}

zx_status_t GroveLcdDevice::GetLineSize(void* ctx, fidl_txn_t* txn) {
    return zircon_display_grove_lcd_LcdGetLineSize_reply(txn, LINE_SIZE - 1);
}

zircon_display_grove_lcd_Lcd_ops_t fidl_lcd_ops = {
    .Clear = GroveLcdDevice::Clear,
    .WriteFirstLine = GroveLcdDevice::WriteFirstLine,
    .WriteSecondLine = GroveLcdDevice::WriteSecondLine,
    .ReadLcd = GroveLcdDevice::ReadLcd,
    .GetLineSize = GroveLcdDevice::GetLineSize,
};

zx_status_t GroveLcdDevice::DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn) {
    zxlogf(INFO, "%s\n", __func__);
    return zircon_display_grove_lcd_Lcd_dispatch(this, txn, msg, &fidl_lcd_ops);
}

zx_status_t GroveLcdDevice::LcdInit() {
    zx_status_t status;
    std::string init ("@Init");

    if (!i2c_lcd.is_valid()) {
        return ZX_ERR_NO_RESOURCES;
    }

    I2cCmd cmd[] = {
        {LCD_CMD, 0x01},
        {LCD_CMD, 0x02},
        {LCD_CMD, 0x0c},
        {LCD_CMD, 0x28},
    };

    fbl::AutoLock lock(&i2c_lock);
    for (const auto& i : cmd) {
        status = i2c_lcd.WriteSync(&i.cmd, sizeof(cmd[0]));
        if (status != ZX_OK) {
            zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
            goto error;
        }
    }

    status = i2c_lcd.WriteSync(reinterpret_cast<const uint8_t*>(init.c_str()), 5);
    if (status != ZX_OK) {
        zxlogf(ERROR, "lcd-cpp: i2c.WriteSync failed\n");
        goto error;
    }

    DdkMakeVisible();

error:
    return status;
}

zx_status_t GroveLcdDevice::Bind(zx_device_t* parent) {
    zxlogf(INFO, "%s\n", __func__);

    fbl::AllocChecker ac;

    auto dev = fbl::make_unique_checked<grove::GroveLcdDevice>(&ac, parent);
    if (!ac.check()) {
        return ZX_ERR_NO_MEMORY;
    }

    /* auto status = dev->DdkAdd("grove-lcd-cpp-drv"); */
    /* auto status = dev->DdkAdd("grove-lcd-cpp-drv", DEVICE_ADD_INVISIBLE); */
    
    /* thrd_t thread;
     * auto ret = thrd_create_with_name(&thread, LcdInit, this, "grove-lcd:Init");
     * if (ret != thrd_success) {
     *     dev->DdkRemove();
     *     return ZX_ERR_INTERNAL;
     * }
     * thrd_detach(thread); */

    auto status = dev->LcdInit();
    status = dev->DdkAdd("grove-lcd-cpp-drv");

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

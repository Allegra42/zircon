#pragma once

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>

#include <zircon/syscalls.h>
#include <zircon/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#if __cplusplus

#include <ddktl/device.h>
#include <ddktl/i2c-channel.h>

#include <fbl/auto_lock.h>
#include <fbl/mutex.h>
#include <fbl/string.h>
#include <fbl/unique_ptr.h>

#endif // __cplusplus

#define LCD_CMD 0x80
#define LINE_SIZE 17

namespace grove {

class GroveLcdDevice;
using DeviceType = ddk::Device<GroveLcdDevice, ddk::Readable, ddk::Writable, ddk::Messageable>;

class GroveLcdDevice : public DeviceType {

public:
    GroveLcdDevice(zx_device_t* parent)
        : DeviceType(parent), i2c_lcd(parent) {}
    ~GroveLcdDevice() {}

    static zx_status_t Bind(zx_device_t* parent);

    // Mixin methods
    void DdkRelease();
    zx_status_t DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual);
    zx_status_t DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual);
    zx_status_t DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn);

    // Definitions for FIDL
    static zx_status_t ClearLcd(void* ctx);
    static zx_status_t WriteFirstLine(void* ctx, uint8_t position, const char* line_data, size_t line_size);
    static zx_status_t WriteSecondLine(void* ctx, uint8_t position, const char* line_data, size_t line_size);
    static zx_status_t ReadLcd(void* ctx, fidl_txn_t* txn);
    static zx_status_t GetLineSize(void* ctx, fidl_txn_t* txn);

private:
    ddk::I2cChannel i2c_lcd;
    fbl::Mutex i2c_lock;
    fbl::String line_one;
    fbl::String line_two;

    struct I2cCmd {
        uint8_t cmd;
        uint8_t val;
    };

    zx_status_t LcdInit();
    zx_status_t WriteLine(I2cCmd* cmds, fbl::String raw_line);
};
}

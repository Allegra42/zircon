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
#include <ddktl/pdev.h>

#include <fbl/auto_lock.h>
#include <fbl/mutex.h>
#include <fbl/string.h>
#include <fbl/unique_ptr.h>

#include <optional>

#endif // __cplusplus

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))

#define RED 0x04
#define GREEN 0x03
#define BLUE 0x02
#define LCD_CMD 0x80

namespace grove {

class GroveDevice;
using DeviceType = ddk::Device<GroveDevice, ddk::Messageable>;

class GroveDevice : public DeviceType {

public:
    GroveDevice(zx_device_t* parent)
        : DeviceType(parent), pdev(parent) {}
    ~GroveDevice() {}

    static zx_status_t Bind(zx_device_t* parent);

    // Mixin methods
    void DdkRelease();
    zx_status_t DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn);

    // Definitions for FIDL
    static zx_status_t SetColor(void* ctx, uint8_t red, uint8_t green, uint8_t blue);
    static zx_status_t GetColor(void* ctx, fidl_txn_t* txn);
    static zx_status_t ClearLcd(void* ctx);
    static zx_status_t WriteFirstLine(void* ctx, uint8_t position, const char* line_data, size_t line_size);
    static zx_status_t WriteSecondLine(void* ctx, uint8_t position, const char* line_data, size_t line_size);
    static zx_status_t ReadLcd(void* ctx, fidl_txn_t* txn);
    static zx_status_t GetLineSize(void* ctx, fidl_txn_t* txn);

private:
    ddk::PDev pdev;
    // different variants possible, use optional I2C in this case
    // make use of the optional I2C devices in whole source code
    std::optional<ddk::I2cChannel> i2c_rgb;
    std::optional<ddk::I2cChannel> i2c_lcd;
    fbl::Mutex i2c_lock;
    uint8_t color_red;
    uint8_t color_green;
    uint8_t color_blue;
    fbl::String line_one;
    fbl::String line_two;

    struct I2cCmd {
        uint8_t cmd;
        uint8_t val;
    };

    zx_status_t PdevInit();
    zx_status_t RgbInit();
    zx_status_t LcdInit();
    zx_status_t WriteLine(I2cCmd* cmds, int cmd_elements, fbl::String raw_line);
};
}

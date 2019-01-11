#pragma once

#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/debug.h>

#include <zircon/types.h>
#include <zircon/syscalls.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#if __cplusplus

#include <ddktl/device.h>
#include <ddktl/i2c-channel.h>

#include <fbl/mutex.h>
#include <fbl/auto_lock.h>
#include <fbl/unique_ptr.h>

#endif // __cplusplus

#define RED 0x04
#define GREEN 0x03
#define BLUE 0x02

namespace grove {

    class GroveRgbDevice;
    using DeviceType = ddk::Device<GroveRgbDevice, ddk::Readable, ddk::Writable, ddk::Messageable>;

    class GroveRgbDevice : public DeviceType {
        
        public:
            GroveRgbDevice(zx_device_t* parent) : DeviceType(parent), i2c_rgb (parent) {}
            ~GroveRgbDevice() {}

            static zx_status_t Bind(zx_device_t* parent);

            // Mixin methods
            void DdkRelease();
            zx_status_t DdkRead(void* buf, size_t count, zx_off_t off, size_t* actual);
            zx_status_t DdkWrite(const void* buf, size_t count, zx_off_t off, size_t* actual);
            zx_status_t DdkMessage(fidl_msg_t* msg, fidl_txn_t* txn);

            // Definitions for FIDL
            static zx_status_t SetColor(void* ctx, uint8_t red, uint8_t green, uint8_t blue);
            static zx_status_t GetColor(void* ctx, fidl_txn_t* txn);
            
        private:
            ddk::I2cChannel i2c_rgb;
            fbl::Mutex i2c_lock;
            uint8_t color_red;
            uint8_t color_green;
            uint8_t color_blue;

            struct I2cCmd {
                uint8_t cmd;
                uint8_t val;
            };
            
            zx_status_t RgbInit();
    };

}

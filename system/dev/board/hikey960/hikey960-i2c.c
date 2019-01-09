// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/platform-defs.h>
#include <soc/hi3660/hi3660-hw.h>

#include <limits.h>

#include "hikey960.h"

static const pbus_mmio_t i2c_mmios[] = {
    {
        // .bus_id = 0 <- position in array. May define an enum for better naming
        .base = MMIO_I2C0_BASE,
        .length = MMIO_I2C0_LENGTH,
    },
    {
        .base = MMIO_I2C1_BASE,
        .length = MMIO_I2C1_LENGTH,
    },
    {
        .base = MMIO_I2C2_BASE,
        .length = MMIO_I2C2_LENGTH,
    },
};

static const pbus_irq_t i2c_irqs[] = {
    {
        .irq = IRQ_IOMCU_I2C0,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
    {
        .irq = IRQ_IOMCU_I2C1,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
    {
        .irq = IRQ_IOMCU_I2C2,
        .mode = ZX_INTERRUPT_MODE_EDGE_HIGH,
    },
};

static const pbus_dev_t i2c_dev = {
    .name = "i2c",
    .vid = PDEV_VID_GENERIC,
    .pid = PDEV_PID_GENERIC,
    .did = PDEV_DID_DW_I2C,
    .mmio_list = i2c_mmios,
    .mmio_count = countof(i2c_mmios),
    .irq_list = i2c_irqs,
    .irq_count = countof(i2c_irqs),
};

static const pbus_i2c_channel_t i2c_grove_rgb_channels[] = {
    {
        .bus_id = 0,        // <-- corresponding to line 14ff mmio array, position 0, may define enum for better readability
        .address = 0x62,    // what's the default address for the grove lcd on I2C? <-- LCD = 0x3e per default, RGB = 0x62 per default.
    },
};

static const pbus_i2c_channel_t i2c_grove_lcd_channels[] = {
    {
        .bus_id = 0,
        .address = 0x3e,
    },
};

static const pbus_dev_t i2c_grove_rgb_dev = {
    .name = "grove-rgb-i2c",
    .vid = PDEV_VID_SEEED,              // specific vid for matching,
    .pid = PDEV_PID_SEEED,              // specific pid for matching,
    .did = PDEV_DID_SEEED_GROVE_RGB,    // specific did for matching,
    .i2c_channel_list = i2c_grove_rgb_channels,
    .i2c_channel_count = countof(i2c_grove_rgb_channels),
};

static const pbus_dev_t i2c_grove_lcd_dev = {
    .name = "grove-lcd-i2c",
    .vid = PDEV_VID_SEEED,              // specific vid for matching,
    .pid = PDEV_PID_SEEED,              // specific pid for matching,
    .did = PDEV_DID_SEEED_GROVE_LCD,    // specific did for matching,
    .i2c_channel_list = i2c_grove_lcd_channels,
    .i2c_channel_count = countof(i2c_grove_lcd_channels),
};


zx_status_t hikey960_i2c_init(hikey960_t* bus) {
    zx_status_t status = pbus_protocol_device_add(&bus->pbus, ZX_PROTOCOL_I2C_IMPL, &i2c_dev);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hikey960_i2c_init: pbus_protocol_device_add failed: %d\n", status);
        return status;
    }

    //add my grove lcd
    if ((status = pbus_device_add(&bus->pbus, &i2c_grove_rgb_dev)) != ZX_OK) {
        zxlogf(ERROR, "i2c_grove_rgb could not be added to pbus. Status: %d\n", status);
    }
    else {
        zxlogf(INFO, "i2c_grove_rgb successfully added!\n");
    }

    if ((status = pbus_device_add(&bus->pbus, &i2c_grove_lcd_dev)) != ZX_OK) {
        zxlogf(ERROR, "i2c_grove_lcd could not be added to pbus. Status: %d\n", status);
    }
    else {
        zxlogf(INFO, "i2c_grove_lcd successfully added!\n");
    }
    return ZX_OK;
}

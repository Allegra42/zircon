#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/platform-defs.h>
#include <stdlib.h>


extern zx_status_t grove_rgb_bind(void* ctx, zx_device_t* parent);

static zx_driver_ops_t grove_rgb_cpp_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = grove_rgb_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(grove-rgb-cpp, grove_rgb_cpp_driver_ops, "grove-rgb-cpp", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_SEEED),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_SEEED),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_SEEED_GROVE_RGB),
ZIRCON_DRIVER_END(grove-rgb-cpp)
// clang-format on

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <zircon/syscalls.h>
#include <lib/fdio/util.h>

#include <zircon/display/grove/rgb/c/fidl.h>

int main (int argc, const char** argv) {

    /* static const char* dev_path = "/dev/sys/platform/3e8:7d0:fa0/grove-rgb-drv"; */
    static const char* dev_path = "/dev/sys/platform/3e8:7d0:fa0/grove-rgb-cpp-drv";

    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "grove-lcd: can not open %s, errno %d (%s)\n", dev_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    zx_handle_t handle;
    zx_status_t status;

    if ((status = fdio_get_service_handle(fd, &handle)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not get fdio service handle, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    if ((status = zircon_display_grove_rgb_RgbSetColor(handle, 0xff, 0x00, 0xff)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);    
    }   

    uint8_t red, green, blue;
    if ((status = zircon_display_grove_rgb_RgbGetColor(handle, &red, &green, &blue)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE); 
    }
    fprintf(stdout, "grove-lcd: color status after change is:\nred: %x\ngreen: %x\nblue: %x\n", red, green, blue);
    exit(EXIT_SUCCESS);
}

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lib/fdio/util.h>
#include <zircon/syscalls.h>

#include <zircon/display/grove/pdev/c/fidl.h>

int main(int argc, const char** argv) {

    static const char* dev_path = "/dev/sys/platform/3e8:7d0:1388/grove-drv";

    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "grove: can not open %s, errno %d (%s)\n", dev_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    zx_handle_t handle;
    zx_status_t status;

    if ((status = fdio_get_service_handle(fd, &handle)) != ZX_OK) {
        fprintf(stderr, "grove: can not get fdio service handle, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    if ((status = zircon_display_grove_pdev_PdevSetColor(handle, 0xff, 0x00, 0xff)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    uint8_t red, green, blue;
    if ((status = zircon_display_grove_pdev_PdevGetColor(handle, &red, &green, &blue)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove: color status after change is:\nred: %x\ngreen: %x\nblue: %x\n", red, green, blue);

    sleep(3);

    if ((status = zircon_display_grove_pdev_PdevClearLcd(handle)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_pdev_PdevWriteFirstLine(handle, 0, "Hallo", 6)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_pdev_PdevWriteFirstLine(handle, 4, "Hallo", 6)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_pdev_PdevWriteSecondLine(handle, 4, "Anna", 6)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    char tmp[40];
    size_t actual;
    if ((status = zircon_display_grove_pdev_PdevReadLcd(handle, tmp, 40, &actual)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove: display content: %s\n", tmp);

    sleep(3);

    uint8_t linesize;
    if ((status = zircon_display_grove_pdev_PdevGetLineSize(handle, &linesize)) != ZX_OK) {
        fprintf(stderr, "grove: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove: linesize %d\n", linesize);

    exit(EXIT_SUCCESS);
}

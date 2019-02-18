#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lib/fdio/util.h>
#include <zircon/syscalls.h>

#include <zircon/display/grove/lcd/c/fidl.h>
#include <zircon/display/grove/rgb/c/fidl.h>

int main(int argc, const char** argv) {

    static const char* rgb_path = "/dev/sys/platform/3e8:7d0:fa0/grove-rgb-drv";
    static const char* lcd_path = "/dev/sys/platform/3e8:7d0:bb8/grove-lcd-drv";

    int fd_rgb = open(rgb_path, O_RDWR);
    if (fd_rgb < 0) {
        fprintf(stderr, "grove-lcd: can not open %s, errno %d (%s)\n", rgb_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    int fd_lcd = open(lcd_path, O_RDWR);
    if (fd_lcd < 0) {
        fprintf(stderr, "grove-lcd: can not open %s, errno %d (%s)\n", lcd_path, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    zx_handle_t handle_rgb;
    zx_handle_t handle_lcd;
    zx_status_t status;

    if ((status = fdio_get_service_handle(fd_rgb, &handle_rgb)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not get fdio service handle, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    if ((status = fdio_get_service_handle(fd_lcd, &handle_lcd)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not get fdio service handle, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    if ((status = zircon_display_grove_rgb_RgbSetColor(handle_rgb, 0x00, 0x00, 0xff)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    uint8_t red, green, blue;
    if ((status = zircon_display_grove_rgb_RgbGetColor(handle_rgb, &red, &green, &blue)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove-lcd: color status after change is:\nred: %x\ngreen: %x\nblue: %x\n", red, green, blue);

    sleep(3);

    if ((status = zircon_display_grove_lcd_LcdClearLcd(handle_lcd)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_lcd_LcdWriteFirstLine(handle_lcd, 0, "Hallo", 6)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_lcd_LcdWriteFirstLine(handle_lcd, 4, "Hallo", 6)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    if ((status = zircon_display_grove_lcd_LcdWriteSecondLine(handle_lcd, 4, "Test", 4)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }

    sleep(3);

    char tmp[40];
    size_t actual;
    if ((status = zircon_display_grove_lcd_LcdReadLcd(handle_lcd, tmp, 40, &actual)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove-lcd: display content: %s\n", tmp);

    sleep(3);

    uint8_t linesize;
    if ((status = zircon_display_grove_lcd_LcdGetLineSize(handle_lcd, &linesize)) != ZX_OK) {
        fprintf(stderr, "grove-lcd: can not execute FIDL command, error %d\n", status);
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "grove-lcd: linesize %d\n", linesize);

    exit(EXIT_SUCCESS);
}

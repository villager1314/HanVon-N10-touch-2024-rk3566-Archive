/*
 * Rockchip RK3566 E-Ink minui backend for the Hanvon N10 Touch 2024.
 *
 * ABI values and conversion behavior were verified against the stock Android
 * 11 recovery and Rockchip's public ebc_dev.h. This code is experimental.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <pixelflinger/pixelflinger.h>

#include "graphics.h"
#include "minuitwrp/minui.h"

namespace {

constexpr int EBC_Y4 = 0;
constexpr int EBC_Y8 = 1;
constexpr unsigned long EBC_GET_BUFFER = 0x7000;
constexpr unsigned long EBC_SEND_BUFFER = 0x7001;
constexpr unsigned long EBC_GET_BUFFER_INFO = 0x7002;
constexpr unsigned long EBC_GET_BUF_FORMAT = 0x7010;
constexpr int EPD_PART_GC16 = 7;
constexpr size_t EBC_MAPPING_SIZE = 0x01400000;

struct ebc_buf_info {
    int offset;
    int epd_mode;
    int height;
    int width;
    int panel_color;
    int win_x1;
    int win_y1;
    int win_x2;
    int win_y2;
    int width_mm;
    int height_mm;
    int dropable;
    char tid_name[16];
    int dma_buf_fd;
};

static_assert(sizeof(ebc_buf_info) == 68, "Rockchip EBC ABI mismatch");

int ebc_fd = -1;
void* ebc_mapping = MAP_FAILED;
int ebc_format = EBC_Y4;
ebc_buf_info panel{};
GRSurface* draw = nullptr;

uint8_t Gray4(const uint8_t* rgbx) {
    return static_cast<uint8_t>(((rgbx[0] * 38 + rgbx[1] * 75 + rgbx[2] * 15) >> 7) >> 4);
}

GRSurface* Init(minui_backend*) {
    ebc_fd = open("/dev/ebc", O_RDWR | O_CLOEXEC);
    if (ebc_fd < 0) return nullptr;

    if (ioctl(ebc_fd, EBC_GET_BUFFER_INFO, &panel) != 0 ||
        ioctl(ebc_fd, EBC_GET_BUF_FORMAT, &ebc_format) != 0 ||
        panel.width <= 0 || panel.height <= 0) {
        close(ebc_fd);
        ebc_fd = -1;
        return nullptr;
    }

    ebc_mapping = mmap(nullptr, EBC_MAPPING_SIZE, PROT_READ | PROT_WRITE,
                       MAP_SHARED, ebc_fd, 0);
    if (ebc_mapping == MAP_FAILED) {
        close(ebc_fd);
        ebc_fd = -1;
        return nullptr;
    }

    draw = static_cast<GRSurface*>(calloc(1, sizeof(GRSurface)));
    if (!draw) return nullptr;
    draw->width = panel.width;
    draw->height = panel.height;
    draw->pixel_bytes = 4;
    draw->row_bytes = panel.width * draw->pixel_bytes;
    draw->format = GGL_PIXEL_FORMAT_RGBX_8888;
    draw->data = static_cast<unsigned char*>(calloc(draw->height, draw->row_bytes));
    if (!draw->data) {
        free(draw);
        draw = nullptr;
        return nullptr;
    }

    printf("Rockchip EBC: %dx%d format=%d mapping=%zu\n",
           panel.width, panel.height, ebc_format, EBC_MAPPING_SIZE);
    return draw;
}

GRSurface* Flip(minui_backend*) {
    if (!draw || ebc_fd < 0 || ebc_mapping == MAP_FAILED) return draw;

    ebc_buf_info update{};
    if (ioctl(ebc_fd, EBC_GET_BUFFER, &update) != 0) {
        perror("Rockchip EBC_GET_BUFFER");
        return draw;
    }

    const int width = std::min(draw->width, update.width);
    const int height = std::min(draw->height, update.height);
    const size_t output_stride = ebc_format == EBC_Y8
                                     ? static_cast<size_t>(update.width)
                                     : static_cast<size_t>((update.width + 1) / 2);
    const size_t required = static_cast<size_t>(update.offset) +
                            output_stride * static_cast<size_t>(update.height);
    if (update.offset < 0 || width <= 0 || height <= 0 || required > EBC_MAPPING_SIZE) {
        fprintf(stderr, "Rockchip EBC: rejected invalid buffer geometry\n");
        return draw;
    }

    auto* output = static_cast<uint8_t*>(ebc_mapping) + update.offset;
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = draw->data + static_cast<size_t>(y) * draw->row_bytes;
        if (ebc_format == EBC_Y8) {
            uint8_t* out = output + static_cast<size_t>(y) * output_stride;
            for (int x = 0; x < width; ++x) out[x] = static_cast<uint8_t>(Gray4(row + x * 4) << 4);
        } else {
            uint8_t* out = output + static_cast<size_t>(y) * output_stride;
            for (int x = 0; x < width; x += 2) {
                const uint8_t low = Gray4(row + x * 4);
                const uint8_t high = x + 1 < width ? Gray4(row + (x + 1) * 4) : low;
                out[x / 2] = static_cast<uint8_t>((high << 4) | low);
            }
        }
    }

    update.epd_mode = EPD_PART_GC16;
    update.win_x1 = 0;
    update.win_y1 = 0;
    update.win_x2 = update.width;
    update.win_y2 = update.height;
    if (ioctl(ebc_fd, EBC_SEND_BUFFER, &update) != 0) perror("Rockchip EBC_SEND_BUFFER");
    return draw;
}

void Blank(minui_backend*, bool) {}

void Exit(minui_backend*) {
    if (draw) {
        free(draw->data);
        free(draw);
        draw = nullptr;
    }
    if (ebc_mapping != MAP_FAILED) {
        munmap(ebc_mapping, EBC_MAPPING_SIZE);
        ebc_mapping = MAP_FAILED;
    }
    if (ebc_fd >= 0) {
        close(ebc_fd);
        ebc_fd = -1;
    }
}

minui_backend backend = { Init, Flip, Blank, Exit };

}  // namespace

minui_backend* open_ebc() {
    return &backend;
}

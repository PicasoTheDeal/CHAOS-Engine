#include "util/screenshot.h"
#include <glad/gl.h>
#include <cstring>
namespace chaos {
std::vector<uint8_t> readFramebufferRgb(int w, int h) {
    std::vector<uint8_t> px(size_t(w) * h * 3), row(size_t(w) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h / 2; y++) {
        uint8_t* a = &px[size_t(y) * w * 3];
        uint8_t* b = &px[size_t(h - 1 - y) * w * 3];
        std::memcpy(row.data(), a, row.size()); std::memcpy(a, b, row.size()); std::memcpy(b, row.data(), row.size());
    }
    return px;
}
}

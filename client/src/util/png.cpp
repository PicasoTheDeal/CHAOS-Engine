#include "util/png.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>
#include <algorithm>
namespace chaos {
std::vector<uint8_t> encodePng(const uint8_t* rgb, int w, int h) {
    std::vector<uint8_t> out;
    stbi_write_png_to_func([](void* ctx, void* data, int size) {
        auto* v = static_cast<std::vector<uint8_t>*>(ctx);
        v->insert(v->end(), static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    }, &out, w, h, 3, rgb, w * 3);
    return out;
}
bool writeFile(const std::string& path, const void* data, size_t len) {
    std::ofstream f(path, std::ios::binary);
    return bool(f.write(static_cast<const char*>(data), std::streamsize(len)));
}

std::vector<uint8_t> shrinkPng(const std::vector<uint8_t>& png, int maxW) {
    int w, h, n;
    unsigned char* px = stbi_load_from_memory(png.data(), int(png.size()), &w, &h, &n, 3);
    if (!px) return {};
    int f = std::max(1, (w + maxW - 1) / maxW);               // integer box factor
    int ow = w / f, oh = h / f;
    std::vector<uint8_t> out(size_t(ow) * oh * 3);
    for (int y = 0; y < oh; y++) for (int x = 0; x < ow; x++) for (int ch = 0; ch < 3; ch++) {
        unsigned sum = 0;
        for (int dy = 0; dy < f; dy++) for (int dx = 0; dx < f; dx++) sum += px[((y * f + dy) * w + (x * f + dx)) * 3 + ch];
        out[(y * ow + x) * 3 + ch] = uint8_t(sum / (f * f));
    }
    stbi_image_free(px);
    return encodePng(out.data(), ow, oh);
}

} // namespace chaos

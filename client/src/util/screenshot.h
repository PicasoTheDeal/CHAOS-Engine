#pragma once
#include <vector>
#include <cstdint>
namespace chaos {
std::vector<uint8_t> readFramebufferRgb(int width, int height);   // bound read framebuffer, rows top-down
}

#pragma once
#include <vector>
#include <string>
#include <cstdint>
namespace chaos {
std::vector<uint8_t> encodePng(const uint8_t* rgb, int w, int h);          // rgb8, top-down rows
bool writeFile(const std::string& path, const void* data, size_t len);
// decode a png (rgb8), box-downscale so the width is at most maxW, re-encode. empty on failure.
std::vector<uint8_t> shrinkPng(const std::vector<uint8_t>& png, int maxW);
}

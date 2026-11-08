#pragma once
// the mark: 16x16 cells. order on the left, falling apart to the right: solid columns 1..4, then cells scattered
// by a fixed noise so the right side is chaos. drawn as small boxes, one per cell, the grid cutting between them.
// one bitmap, drawn everywhere: the window icon, the panel brand, the poster, and (as rects) the site's favicon,
// the packaged icon and the hero. loose(x) says which cells may drift when animated.
#include <vector>
#include <cstdint>
namespace chaos::mark {
inline constexpr int SIZE = 16;
inline constexpr const char* ROWS[SIZE] = {
    "................",
    "................",
    ".#######..##....",
    ".#####...##.....",
    ".#######.#......",
    ".#######.##..#..",
    ".#######.#......",
    ".##########.....",
    ".######.#.#.....",
    ".#####.###...#..",
    ".######..##.....",
    ".######..###.#..",
    ".#######..#.##..",
    ".#######...#....",
    "................",
    "................",
};
inline bool on(int x, int y) { return x >= 0 && y >= 0 && x < SIZE && y < SIZE && ROWS[y][x] == '#'; }
inline bool loose(int x) { return x >= 5; }
// rgba pixels at an integer scale, each cell a box with a one-unit gap when scale >= 4, transparent background
inline std::vector<uint8_t> rgba(int scale, uint8_t r, uint8_t g, uint8_t b) {
    const int n = SIZE * scale, gap = scale >= 4 ? 1 : 0;
    std::vector<uint8_t> px(size_t(n) * n * 4, 0);
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) {
        int cx = x / scale, cy = y / scale, ix = x % scale, iy = y % scale;
        if (!on(cx, cy) || ix < gap || iy < gap || ix >= scale - gap || iy >= scale - gap) continue;
        uint8_t* p = &px[(size_t(y) * n + x) * 4]; p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
    }
    return px;
}
}

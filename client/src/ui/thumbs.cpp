#include "ui/thumbs.h"
#include <glad/gl.h>
#include <vector>
#include <algorithm>
#include "stb_image.h"
namespace chaos {
Thumbs::~Thumbs() { clear(); }
void Thumbs::clear() { for (auto& [k, t] : cache_) glDeleteTextures(1, &t.tex); cache_.clear(); }
// the vault keeps full-size pngs; the panel shows them at 144 px. decode once, box-filter down to at most 320 wide
// before upload: a 1080p preview went from 8 MB of texture to about 300 KB, and no mip chain is needed
unsigned Thumbs::get(const std::string& path, int& w, int& h) {
    auto it = cache_.find(path);
    if (it != cache_.end()) { w = it->second.w; h = it->second.h; return it->second.tex; }
    int n, sw, sh;
    unsigned char* px = stbi_load(path.c_str(), &sw, &sh, &n, 3);
    T t{ 0, 0, 0 };
    if (px) {
        const int f = std::max(1, sw / 320);
        w = sw / f; h = sh / f;
        std::vector<unsigned char> small(size_t(w) * h * 3);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (int yy = 0; yy < f; yy++) for (int xx = 0; xx < f; xx++) { const unsigned char* s = px + ((size_t(y * f + yy) * sw) + (x * f + xx)) * 3; r += s[0]; g += s[1]; b += s[2]; }
            unsigned char* d = &small[(size_t(y) * w + x) * 3]; const unsigned area = f * f; d[0] = r / area; d[1] = g / area; d[2] = b / area;
        }
        stbi_image_free(px);
        t.w = w; t.h = h;
        glGenTextures(1, &t.tex);
        glBindTexture(GL_TEXTURE_2D, t.tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, small.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else { w = h = 0; }
    cache_[path] = t;
    return t.tex;
}
}

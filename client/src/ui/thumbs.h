#pragma once
#include <string>
#include <unordered_map>
namespace chaos {
// png -> gl texture, cached by path. 0 when the file cannot be read.
class Thumbs {
public:
    ~Thumbs();
    unsigned get(const std::string& path, int& w, int& h);
    void clear();
private:
    struct T { unsigned tex; int w, h; };
    std::unordered_map<std::string, T> cache_;
};
}

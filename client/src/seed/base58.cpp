#include "seed/base58.h"
#include <stdexcept>
#include <cstring>
namespace chaos {
static const char* ALPHA = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
std::string base58Encode(const uint8_t* data, size_t len) {
    size_t zeros = 0; while (zeros < len && data[zeros] == 0) zeros++;
    std::vector<uint8_t> digits(len * 138 / 100 + 1, 0);
    size_t length = 0;
    for (size_t i = zeros; i < len; i++) {
        int carry = data[i];
        size_t j = 0;
        for (auto it = digits.rbegin(); (carry != 0 || j < length) && it != digits.rend(); ++it, ++j) {
            carry += 256 * (*it); *it = uint8_t(carry % 58); carry /= 58;
        }
        length = j;
    }
    std::string out(zeros, '1');
    auto it = digits.begin() + (digits.size() - length);
    while (it != digits.end() && *it == 0) ++it;
    for (; it != digits.end(); ++it) out += ALPHA[*it];
    return out;
}
std::vector<uint8_t> base58Decode(std::string_view s) {
    size_t zeros = 0; while (zeros < s.size() && s[zeros] == '1') zeros++;
    std::vector<uint8_t> bytes(s.size() * 733 / 1000 + 1, 0);
    size_t length = 0;
    for (size_t i = zeros; i < s.size(); i++) {
        const char* p = std::strchr(ALPHA, s[i]);
        if (!p || !s[i]) throw std::runtime_error("bad base58 char");
        int carry = int(p - ALPHA);
        size_t j = 0;
        for (auto it = bytes.rbegin(); (carry != 0 || j < length) && it != bytes.rend(); ++it, ++j) {
            carry += 58 * (*it); *it = uint8_t(carry % 256); carry /= 256;
        }
        length = j;
    }
    std::vector<uint8_t> out(zeros, 0);
    auto it = bytes.begin() + (bytes.size() - length);
    while (it != bytes.end() && *it == 0) ++it;
    out.insert(out.end(), it, bytes.end());
    return out;
}
}

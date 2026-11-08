#pragma once
// a span of milliseconds as people say it: "2 w 3 d", "6 d 14 h", "3 h 20 min", "45 min", "under a minute".
// the two largest non-zero units, nothing smaller than a minute.
#include <string>
namespace chaos {
inline std::string humanMs(long long ms) {
    if (ms <= 0) return "now";
    long long m = (ms + 59'999) / 60'000;                 // round up: "next in 1 min" never says 0
    if (m < 1) return "under a minute";
    long long w = m / 10'080, d = (m % 10'080) / 1'440, h = (m % 1'440) / 60, mi = m % 60;
    auto u = [](long long n, const char* unit) { return std::to_string(n) + " " + unit; };
    if (w > 0) return d > 0 ? u(w, "w") + " " + u(d, "d") : u(w, "w");
    if (d > 0) return h > 0 ? u(d, "d") + " " + u(h, "h") : u(d, "d");
    if (h > 0) return mi > 0 ? u(h, "h") + " " + u(mi, "min") : u(h, "h");
    return u(mi, "min");
}
}

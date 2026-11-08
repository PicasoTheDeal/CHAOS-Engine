#include "platform/power.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace chaos::power {
bool onBattery() {
    namespace fs = std::filesystem;
    try {
        std::error_code ec;
        for (const auto& e : fs::directory_iterator("/sys/class/power_supply", ec)) {
            std::ifstream st(e.path() / "status"); std::string s; std::getline(st, s);
            if (s == "Discharging") return true;
        }
    } catch (...) {}
    return false;
}
}

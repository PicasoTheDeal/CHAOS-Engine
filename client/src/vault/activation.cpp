#include "vault/activation.h"
#include "vault/vault.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace chaos::activation {

using json = nlohmann::json;

std::string path() { return (vault::dir().parent_path() / "activated.json").string(); }

std::map<std::string, Entry> load() {
    std::map<std::string, Entry> out;
    std::ifstream in(path());
    if (!in) return out;
    try {
        json j = json::parse(in);
        for (auto& [k, v] : j.items())
            out[k] = Entry{ v.value("at_ms", 0ULL), v.value("owner", ""), v.value("state", ""), v.value("current", false), v.value("value_cents", 0LL), v.value("nonce", "") };
    } catch (...) {}
    return out;
}

void record(const Seed32& seed, const Entry& e) {
    auto all = load();
    all[toHex(seed)] = e;
    json j = json::object();
    for (auto& [k, v] : all) j[k] = { { "at_ms", v.atMs }, { "owner", v.owner }, { "state", v.state }, { "current", v.current }, { "value_cents", v.valueCents }, { "nonce", v.nonceHex } };
    std::ofstream(path()) << j.dump(2) << '\n';
}

std::optional<Entry> get(const Seed32& seed) { auto all = load(); auto it = all.find(toHex(seed)); if (it == all.end()) return std::nullopt; return it->second; }
bool isActivated(const Seed32& seed) { return get(seed).has_value(); }

} // namespace chaos::activation

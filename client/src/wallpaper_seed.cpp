#include "wallpaper_seed.h"
#include <exception>
#include "vault/activation.h"
#include "vault/vault.h"
#include <cstring>
#include "blake3.h"

namespace chaos {

std::optional<Seed32> pickWallpaperSeed(const std::string& seedArg, bool online, std::string& note) {
    note.clear();
    try {
        if (!seedArg.empty()) {
            Seed32 s;
            try { s = seedFromString(seedArg); } catch (const std::exception& e) { note = std::string("bad seed: ") + e.what(); return std::nullopt; }
            if (!online && !activation::isActivated(s) && !vault::has(s)) { note = "offline: this seed is not in the vault and was never loaded online on this install. keep it, activate it, or reconnect."; return std::nullopt; }
            return s;
        }
        if (online) return randomSeed();
        auto act = activation::load();
        if (!act.empty()) {
            Seed32 s = seedFromHex(act.begin()->first);
            note = "offline: playing an activated chaos " + toShortHex(s);
            return s;
        }
        auto kept = vault::list();                      // newest first
        if (!kept.empty()) {
            Seed32 s; std::memcpy(s.data(), kept.front().file.seed_full, 32);
            note = "offline: playing from the vault " + toShortHex(s);
            return s;
        }
        note = "offline and the vault is empty: roll something in the chaos app, or reconnect.";
        return std::nullopt;
    } catch (const std::exception& e) { note = std::string("seed rule failed: ") + e.what(); return std::nullopt; }
    catch (...) { note = "seed rule failed"; return std::nullopt; }
}

Seed32 lobbySeed() {
    static const char tag[] = "CHAOS-LOBBY-v1";
    blake3_hasher h; blake3_hasher_init(&h);
    blake3_hasher_update(&h, tag, sizeof(tag) - 1);
    Seed32 s; blake3_hasher_finalize(&h, s.data(), 32); return s;
}

} // namespace chaos

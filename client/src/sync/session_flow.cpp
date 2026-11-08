#include "sync/session_flow.h"
#include "vault/vault.h"
#include "vault/activation.h"
#include "server_pubkey.h"
#include "util/png.h"
#include <cstring>
#include <fstream>
#include <iterator>

namespace chaos::flow {

api::Result<ClaimableRoll> rollClaimable(const std::string& registry, const std::string& token) {
    ClaimableRoll r{};
    cf::randomBytes(r.clientRandom, 32);
    auto commit = cf::commitOf(r.clientRandom);
    auto t = api::ticket(registry, token, commit.data());
    api::Result<ClaimableRoll> out; out.offline = t.offline; out.error = t.error;
    if (!t) return out;
    if (!cf::verifyTicketSig(*t.value, CHAOS_SERVER_PUBKEY)) { out.error = api::Error{ "the ticket does not check against the registry key we ship", 0 }; return out; }
    if (std::memcmp(t.value->commit, commit.data(), 32) != 0) { out.error = api::Error{ "the ticket carries a different commit", 0 }; return out; }
    r.ticket = *t.value;
    r.seed = cf::deriveSeed(r.ticket, r.clientRandom);
    out.value = r;
    return out;
}

Outcome claimFile(const std::string& registry, const std::string& token, const chaos_file_t& rolled) {
    if (rolled.state != CHAOS_STATE_ROLLED) return { false, "only a rolled file can be claimed" };
    api::Row row;
    // the preview png the vault keeps next to the file goes up with the claim, if it is there and not huge
    std::vector<uint8_t> png;
    { std::ifstream in(vault::pathFor(rolled).replace_extension(".png"), std::ios::binary); if (in) png.assign(std::istreambuf_iterator<char>(in), {}); }
    if (!png.empty()) png = shrinkPng(png, PREVIEW_UPLOAD_WIDTH);           // a market thumbnail, not the wallpaper
    if (png.size() > PREVIEW_UPLOAD_MAX_BYTES) png.clear();
    auto res = api::claim(registry, token, reinterpret_cast<const uint8_t*>(&rolled), sizeof rolled, &row, png.empty() ? nullptr : &png);
    if (!res) return { false, res.error ? res.error->message : "claim failed" };
    chaos_file_t signedFile{}; std::memcpy(&signedFile, res.value->data(), 512);
    auto c = cf::verify(signedFile);
    if (!c.ok()) return { false, "the registry returned a file that does not verify: " + c.why };
    if (std::memcmp(signedFile.seed_full, rolled.seed_full, 32) != 0) return { false, "the registry returned a different seed" };
    auto kept = vault::store(signedFile, {});
    char buf[160]; std::snprintf(buf, sizeof buf, "claimed: tier %d, value %lld.%02lld, owner %s", row.tier, row.valueCents / 100, row.valueCents % 100, row.owner.c_str());
    std::string where = kept.chaosPath.string();
    std::printf("%s\nfile   %s\n", buf, where.c_str());
    return { true, std::string(buf) + "  /  file: " + where };
    return { true, buf };
}

SyncReport syncVault(const std::string& registry, const std::string& token) {
    SyncReport rep;
    auto entries = vault::list();
    std::vector<std::string> seeds;
    for (const auto& e : entries) if (e.file.state != CHAOS_STATE_PREVIEW) { Seed32 s; std::memcpy(s.data(), e.file.seed_full, 32); seeds.push_back(toHex(s)); }
    if (!seeds.empty()) {
        auto st = api::sync(registry, token, seeds);
        if (!st) { rep.message = st.error ? st.error->message : "sync failed"; return rep; }
        for (const auto& e : entries) {
            if (e.file.state == CHAOS_STATE_PREVIEW) continue;
            Seed32 s; std::memcpy(s.data(), e.file.seed_full, 32);
            auto it = st.value->find(toHex(s));
            if (it == st.value->end() || !it->second.known) { rep.unknown++; continue; }
            rep.checked++;
            bool mine = it->second.current;
            if (mine) rep.current++; else rep.stale++;
            auto prev = activation::get(s);
            activation::record(s, { cf::nowMs(), it->second.owner, it->second.state, mine, prev ? prev->valueCents : 0, "" });
        }
    }
    // seeds the registry says are mine that I do not hold (acquired on the web): fetch the signed bytes
    auto mine = api::vault(registry, token);
    if (mine) {
        for (const auto& row : *mine.value) {
            bool have = false;
            for (const auto& e : entries) { Seed32 s; std::memcpy(s.data(), e.file.seed_full, 32); if (toHex(s) == row.seedHex && e.file.state == CHAOS_STATE_CLAIMED) { have = true; break; } }
            if (have) continue;
            auto f = api::file(registry, token, row.seedHex);
            if (!f) continue;
            chaos_file_t cfile{}; std::memcpy(&cfile, f.value->data(), 512);
            if (!cf::verify(cfile).ok()) continue;
            vault::store(cfile, {});
            Seed32 s; std::memcpy(s.data(), cfile.seed_full, 32);
            activation::record(s, { cf::nowMs(), row.owner, row.state, true, row.valueCents, row.nonceHex });
            rep.fetched++;
        }
    }
    char buf[200]; std::snprintf(buf, sizeof buf, "synced: %d checked, %d current, %d transferred away, %d unknown, %d fetched", rep.checked, rep.current, rep.stale, rep.unknown, rep.fetched);
    rep.message = buf;
    return rep;
}

} // namespace chaos::flow

#include "sync/api.h"
#include "server_pubkey.h"
#include "vault/vault.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>

namespace chaos::api {

using json = nlohmann::json;

// timeouts, ms. health is short because the app waits on it at start.
constexpr long T_HEALTH = 2500, T_SHORT = 8000, T_LONG = 15000;

static void init() { static std::once_flag f; std::call_once(f, [] { curl_global_init(CURL_GLOBAL_DEFAULT); }); }
static size_t sink(char* p, size_t s, size_t n, void* u) { static_cast<std::string*>(u)->append(p, s * n); return s * n; }

struct Http { long status = 0; std::string body; bool transport = false; };

static Http request(const std::string& url, const char* method, const std::string* body, const std::string* bearer, long timeoutMs) {
    init();
    Http r;
    CURL* c = curl_easy_init();
    if (!c) return r;
    struct curl_slist* hdr = nullptr;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, sink);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, timeoutMs);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, timeoutMs);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "chaos-engine/" CHAOS_VERSION_STRING);
    curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method);
    hdr = curl_slist_append(hdr, "content-type: application/json");
    if (bearer) hdr = curl_slist_append(hdr, ("authorization: Bearer " + *bearer).c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    if (body) { curl_easy_setopt(c, CURLOPT_POSTFIELDS, body->c_str()); curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, long(body->size())); }
    CURLcode rc = curl_easy_perform(c);
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    r.transport = rc == CURLE_OK;
    return r;
}

std::string defaultRegistryUrl() {
    const char* e = std::getenv("CHAOS_REGISTRY");
    return (e && *e) ? std::string(e) : CHAOS_REGISTRY_URL;
}

static std::string b64(const uint8_t* d, size_t n) {
    static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; o.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = uint32_t(d[i]) << 16 | (i + 1 < n ? uint32_t(d[i + 1]) << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        o += A[v >> 18]; o += A[(v >> 12) & 63]; o += i + 1 < n ? A[(v >> 6) & 63] : '='; o += i + 2 < n ? A[v & 63] : '=';
    }
    return o;
}
static std::vector<uint8_t> unb64(const std::string& s) {
    static int T[256]; static bool init = false;
    if (!init) { std::memset(T, -1, sizeof T); const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; for (int i = 0; i < 64; i++) T[int(A[i])] = i; init = true; }
    std::vector<uint8_t> out; uint32_t v = 0; int bits = 0;
    for (unsigned char ch : s) { if (T[ch] < 0) continue; v = (v << 6) | uint32_t(T[ch]); bits += 6; if (bits >= 8) { bits -= 8; out.push_back(uint8_t((v >> bits) & 255)); } }
    return out;
}
static std::string hex(const uint8_t* d, size_t n) { std::string s(n * 2, '0'); for (size_t i = 0; i < n; i++) std::snprintf(&s[2 * i], 3, "%02x", d[i]); return s; }

template <class T> static Result<T> fromHttp(const Http& h, const std::function<T(const json&)>& parse) {
    Result<T> r;
    if (!h.transport) { r.offline = true; r.error = Error{ "registry unreachable", 0 }; return r; }
    json j; try { j = json::parse(h.body); } catch (...) { r.error = Error{ "bad json from registry", h.status }; return r; }
    if (h.status < 200 || h.status >= 300 || j.contains("error") || (h.status >= 400 && j.contains("message"))) {
        r.error = Error{ j.value("error", j.value("message", std::string("http ") + std::to_string(h.status))), h.status }; return r;
    }
    try { r.value = parse(j); } catch (const std::exception& e) { r.error = Error{ std::string("unexpected reply: ") + e.what(), h.status }; }
    return r;
}

std::optional<RegistryInfo> health(const std::string& base, long timeoutMs) {
    if (timeoutMs <= 0) timeoutMs = T_HEALTH;
    auto h = request(base + "/api/health", "GET", nullptr, nullptr, timeoutMs);
    if (!h.transport || h.status != 200) return std::nullopt;
    try {
        json j = json::parse(h.body);
        RegistryInfo info{ j.value("pubkey", "") };
        if (info.pubkeyHex != hex(CHAOS_SERVER_PUBKEY, 32)) return std::nullopt;   // a registry with another key is not our registry
        return info;
    } catch (...) { return std::nullopt; }
}

std::optional<CheckResult> check(const std::string& base, const uint8_t* file, size_t len, long timeoutMs) {
    std::string body = json{ { "file", b64(file, len) } }.dump();
    auto h = request(base + "/api/check", "POST", &body, nullptr, timeoutMs);
    if (!h.transport) return std::nullopt;
    try {
        json j = json::parse(h.body);
        if (j.contains("error")) return CheckResult{ false, j["error"].get<std::string>(), false, "", "", false, 0 };
        CheckResult c{};
        c.why = j["check"].value("why", ""); c.valid = c.why.empty();
        const auto& reg = j["registry"]; c.known = reg.value("known", false);
        if (c.known) { c.owner = reg.value("owner", ""); c.state = reg.value("state", ""); c.current = reg.value("current", false); c.valueCents = reg.value("value_cents", 0LL); }
        return c;
    } catch (...) { return std::nullopt; }
}

// ---- session ------------------------------------------------------------------------------

static std::filesystem::path sessionPath() { return vault::configDir() / "session"; }
std::optional<std::string> sessionToken() {
    std::ifstream in(sessionPath()); std::string t; std::getline(in, t);
    if (t.empty()) return std::nullopt;
    return t;
}
bool saveSession(const std::string& token) {
    auto p = sessionPath();
    { std::ofstream out(p); if (!(out << token << '\n')) return false; }
    ::chmod(p.c_str(), 0600);
    return true;
}
bool clearSession() { std::error_code ec; std::filesystem::remove(sessionPath(), ec); return !ec; }

// ---- authed calls --------------------------------------------------------------------------

Result<std::string> devLogin(const std::string& base, const std::string& name) {
    std::string body = json{ { "id_token", name } }.dump();
    return fromHttp<std::string>(request(base + "/api/auth/google", "POST", &body, nullptr, T_SHORT), [](const json& j) { return j.at("token").get<std::string>(); });
}

Result<Me> me(const std::string& base, const std::string& tok) {
    return fromHttp<Me>(request(base + "/api/me", "GET", nullptr, &tok, T_SHORT), [](const json& j) {
        const auto& a = j.at("allowance");
        return Me{ j.value("id", ""), j.value("email", ""), j.value("name", ""), a.value("banked", 0), a.value("max", 14), a.value("nextInMs", 0LL) };
    });
}

Result<cf::Ticket> ticket(const std::string& base, const std::string& tok, const uint8_t commit[32]) {
    std::string body = json{ { "commit", hex(commit, 32) } }.dump();
    return fromHttp<cf::Ticket>(request(base + "/api/ticket", "POST", &body, &tok, T_SHORT), [](const json& j) {
        auto bytes = unb64(j.at("ticket").get<std::string>());
        if (bytes.size() != sizeof(cf::Ticket)) throw std::runtime_error("ticket is not 160 bytes");
        cf::Ticket t; std::memcpy(&t, bytes.data(), sizeof t); return t;
    });
}

static Row rowOf(const json& j) {
    return Row{ j.value("seed", ""), j.value("owner", ""), j.value("state", ""), j.value("nonce", ""), j.value("family", 0), j.value("tier", 0), j.value("mode", 0), j.value("value_cents", 0LL) };
}

Result<std::vector<uint8_t>> claim(const std::string& base, const std::string& tok, const uint8_t* file, size_t len, Row* rowOut, const std::vector<uint8_t>* png) {
    json req = { { "file", b64(file, len) } };
    if (png && !png->empty()) req["preview"] = b64(png->data(), png->size());
    std::string body = req.dump();
    return fromHttp<std::vector<uint8_t>>(request(base + "/api/claim", "POST", &body, &tok, T_LONG), [&](const json& j) {
        if (rowOut) *rowOut = rowOf(j);
        auto bytes = unb64(j.at("file").get<std::string>());
        if (bytes.size() != 512) throw std::runtime_error("claimed file is not 512 bytes");
        return bytes;
    });
}

Result<std::vector<Row>> vault(const std::string& base, const std::string& tok) {
    return fromHttp<std::vector<Row>>(request(base + "/api/vault", "GET", nullptr, &tok, T_SHORT), [](const json& j) { std::vector<Row> v; for (const auto& r : j) v.push_back(rowOf(r)); return v; });
}

Result<std::map<std::string, SyncState>> sync(const std::string& base, const std::string& tok, const std::vector<std::string>& seeds) {
    std::string body = json{ { "seeds", seeds } }.dump();
    return fromHttp<std::map<std::string, SyncState>>(request(base + "/api/sync", "POST", &body, &tok, T_LONG), [](const json& j) {
        std::map<std::string, SyncState> m;
        for (auto& [k, v] : j.items()) m[k] = v.is_null() ? SyncState{ false, "", "", false } : SyncState{ true, v.value("state", ""), v.value("owner", ""), v.value("current", false) };
        return m;
    });
}

Result<std::vector<uint8_t>> file(const std::string& base, const std::string& tok, const std::string& seedHex) {
    auto h = request(base + "/api/file/" + seedHex, "GET", nullptr, &tok, T_LONG);
    Result<std::vector<uint8_t>> r;
    if (!h.transport) { r.offline = true; r.error = Error{ "registry unreachable", 0 }; return r; }
    if (h.status != 200 || h.body.size() != 512) { r.error = Error{ "no file (" + std::to_string(h.status) + ")", h.status }; return r; }
    r.value = std::vector<uint8_t>(h.body.begin(), h.body.end());
    return r;
}

} // namespace chaos::api

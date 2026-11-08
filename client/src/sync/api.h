#pragma once
// the app's side of the registry api. health + check are public; the rest ride a bearer session token.
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <cstdint>
#include "chaos_file/format.h"

namespace chaos::api {

struct RegistryInfo { std::string pubkeyHex; };
struct CheckResult { bool valid; std::string why; bool known; std::string owner; std::string state; bool current; long long valueCents; };
struct Me { std::string id, email, name; int banked, max; long long nextInMs; };
struct Row { std::string seedHex, owner, state, nonceHex; int family, tier, mode; long long valueCents; };
struct SyncState { bool known; std::string state, owner; bool current; };
struct Error { std::string message; long status; };
template <class T> struct Result { std::optional<T> value; std::optional<Error> error; bool offline = false; explicit operator bool() const { return value.has_value(); } };

std::string defaultRegistryUrl();   // CHAOS_REGISTRY env, else https://chaos-eng.tetstack.dev

std::optional<RegistryInfo> health(const std::string& baseUrl, long timeoutMs = 2500);
std::optional<CheckResult> check(const std::string& baseUrl, const uint8_t* file, size_t len, long timeoutMs = 5000);

// session: ~/.config/chaos/session (0600). the token comes from <registry>/login?app=1 (paste), or the dev login.
std::optional<std::string> sessionToken();
bool saveSession(const std::string& token);
bool clearSession();

Result<std::string> devLogin(const std::string& baseUrl, const std::string& name);              // ALLOW_FAKE_LOGIN registries only
Result<Me> me(const std::string& baseUrl, const std::string& token);
Result<cf::Ticket> ticket(const std::string& baseUrl, const std::string& token, const uint8_t commit[32]);
Result<std::vector<uint8_t>> claim(const std::string& baseUrl, const std::string& token, const uint8_t* file, size_t len, Row* rowOut = nullptr, const std::vector<uint8_t>* previewPng = nullptr);
Result<std::vector<Row>> vault(const std::string& baseUrl, const std::string& token);
Result<std::map<std::string, SyncState>> sync(const std::string& baseUrl, const std::string& token, const std::vector<std::string>& seedHexes);
Result<std::vector<uint8_t>> file(const std::string& baseUrl, const std::string& token, const std::string& seedHex);

} // namespace chaos::api

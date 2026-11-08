#pragma once
// "once loaded online, usable offline. never generated offline."
// activated.json next to the vault: the seeds the registry has confirmed on this install, with what it said.
#include <string>
#include <map>
#include <optional>
#include <cstdint>
#include "seed/seed.h"

namespace chaos::activation {

struct Entry { uint64_t atMs; std::string owner; std::string state; bool current; long long valueCents; std::string nonceHex; };

std::map<std::string, Entry> load();                       // seed hex -> entry
void record(const Seed32& seed, const Entry& e);           // upsert + save
std::optional<Entry> get(const Seed32& seed);
bool isActivated(const Seed32& seed);
std::string path();

} // namespace chaos::activation

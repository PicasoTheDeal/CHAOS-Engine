#pragma once
// The local vault: ~/.local/share/chaos/vault/<short>.chaos + .png. Plain files. The file is a receipt, the
// website is the truth. v0.4 writes preview-state files only (no ticket, no signatures); v0.5 adds the rest.
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include "scene/dimension.h"
#include "chaos_format.h"
#include "chaos_file/format.h"

namespace chaos::vault {

std::filesystem::path dir();                       // the chosen folder (setDir / settings.json vault_dir), else $XDG_DATA_HOME/chaos/vault. created on demand
void setDir(const std::filesystem::path& p);       // empty = back to the default
std::filesystem::path configDir();                 // $XDG_CONFIG_HOME/chaos
std::array<uint8_t, 16> instanceId();              // read or create $XDG_CONFIG_HOME/chaos/instance

// Fills a chaos_file_t from a rolled dimension. With a ticket (and the client_random that produced the seed) the
// state is ROLLED, otherwise PREVIEW. The preview png bytes are hashed into preview_hash.
chaos_file_t makeFile(const Dimension& d, const std::vector<uint8_t>& png, const cf::Ticket* ticket = nullptr, const uint8_t* clientRandom = nullptr);

struct KeptEntry { std::filesystem::path chaosPath, pngPath; };
KeptEntry keep(const Dimension& d, const std::vector<uint8_t>& png, const cf::Ticket* ticket = nullptr, const uint8_t* clientRandom = nullptr);
KeptEntry store(const chaos_file_t& f, const std::vector<uint8_t>& png);   // any file (eg a claimed one coming back from the registry)
std::filesystem::path pathFor(const chaos_file_t& f);

struct Entry { std::string name; chaos_file_t file; };
std::vector<Entry> list();                          // every parseable .chaos in the vault
bool has(const Seed32& seed);                       // is this seed kept here (any state)

} // namespace chaos::vault

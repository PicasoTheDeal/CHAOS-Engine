#pragma once
// the wallpaper's seed rule, shared by the app's wallpaper mode and the plasma plugin (so it lives in chaos_core,
// which both link; the plugin once referenced it from the app binary and plasmashell refused to load the plugin).
#include <string>
#include <optional>
#include "seed/seed.h"
namespace chaos {
// online: a given seed plays, empty rolls a random one. offline: a given seed must be kept in this install's vault
// or activated here (showing again what this machine already rolled and showed is not generation); empty plays the
// first activated one, else the newest kept one. nullopt means nothing may play; `note` says why, or what was picked.
// never throws.
std::optional<Seed32> pickWallpaperSeed(const std::string& seedArg, bool online, std::string& note);
// the lobby: one fixed, public seed that is nobody's and never claimable. the plugin shows it instead of a black
// desktop when nothing may play. a constant is not generation, so the offline rule stays whole.
Seed32 lobbySeed();
}

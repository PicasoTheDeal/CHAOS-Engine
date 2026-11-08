#pragma once
// the three things the app does with the registry beyond health/check, wired so the cli and the panel share them.
#include <string>
#include <optional>
#include <functional>
#include "seed/seed.h"
#include "chaos_file/format.h"
#include "sync/api.h"

namespace chaos::flow {

// the preview that rides with a claim: downscaled to this width, dropped if still bigger than the registry's cap
constexpr int PREVIEW_UPLOAD_WIDTH = 640;
constexpr size_t PREVIEW_UPLOAD_MAX_BYTES = 1'500'000;   // mirrors PREVIEW_MAX_BYTES on the registry

struct Outcome { bool ok; std::string message; };

// commit -> ticket -> seed. the reveal (client_random) and the ticket come back for the roll and the kept file.
struct ClaimableRoll { Seed32 seed; cf::Ticket ticket; uint8_t clientRandom[32]; };
api::Result<ClaimableRoll> rollClaimable(const std::string& registry, const std::string& token);

// claim a rolled vault file: POST, verify what comes back, store it over the rolled one, activate it.
Outcome claimFile(const std::string& registry, const std::string& token, const chaos_file_t& rolled);

// sync the vault: every local seed's state from the registry, plus any seed the registry says you own but you do not hold.
struct SyncReport { int checked = 0, current = 0, stale = 0, unknown = 0, fetched = 0; std::string message; };
SyncReport syncVault(const std::string& registry, const std::string& token);

} // namespace chaos::flow

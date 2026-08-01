#pragma once

#include <string>
#include <vector>

#include "mega/Types.h"

namespace mega {

/// Authoritative state pushed by a server. It mirrors the wire format used by
/// both transports (JSON over WebSocket for the JS Edition, the same JSON over
/// plain TCP for the native editions).
struct Snapshot {
    MatchState match;
    std::vector<Player> players;
    std::vector<Projectile> projectiles;
    std::vector<Gem> gems;
    bool valid = false;
};

}  // namespace mega

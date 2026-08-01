#pragma once

#include "mega/World.h"

namespace mega {

/// Decides what a bot-controlled player wants to do this tick: grab gems when
/// the team is behind, hunt the closest visible enemy, and retreat when hurt.
PlayerInput decideBotInput(const World& world, const Player& self);

/// Applies `decideBotInput` to every bot in the world.
void driveBots(World& world);

}  // namespace mega

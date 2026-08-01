#pragma once

#include <memory>

#include "mega/Types.h"
#include "mega/World.h"
#include "mega/app/Profile.h"

namespace mega::app {

/// Everything a frontend has to implement. The game loop never talks to SDL,
/// a terminal or a mobile surface directly.
class Renderer {
public:
    virtual ~Renderer() = default;

    /// Reads platform input and fills `out`. Returns false when the player
    /// asked to quit.
    virtual bool pollInput(PlayerInput& out) = 0;

    virtual void draw(const World& world, EntityId localPlayer) = 0;

    virtual void shutdown() {}
};

std::unique_ptr<Renderer> makeAsciiRenderer(const Profile& profile);

/// Returns nullptr when the binary was built without SDL2 support.
std::unique_ptr<Renderer> makeSdlRenderer(const Profile& profile);

}  // namespace mega::app
